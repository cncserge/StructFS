#pragma once
#include <Arduino.h>
#include <FS.h>
#include <type_traits>

namespace StructFS {

template <typename T>
class Storage {
    static_assert(std::is_trivially_copyable<T>::value,
                  "T must be trivially copyable (no String, vector, pointers, etc.)");

  public:
    using DefaultsFn = void (*)(T&);

    struct Header {
        uint32_t magic;    // сигнатура
        uint16_t version;  // версия структуры
        uint16_t dataSize; // размер T
        uint32_t crc;      // CRC32 от данных
    } __attribute__((packed));

  private:
    fs::FS&   fs_;
    const char* path_;
    uint32_t magic_;
    uint16_t version_;
    DefaultsFn defaultsFn_;
    bool     valid_ = false;
    T        data_{};

  public:
    // magic можно задать свой, по умолчанию 'CFG1'
    explicit Storage(fs::FS& fs,
                     const char* path,
                     uint16_t version,
                     uint32_t magic = 0x31474643UL, // 'CFG1'
                     DefaultsFn defaultsFn = nullptr)
        : fs_(fs)
        , path_(path)
        , magic_(magic)
        , version_(version)
        , defaultsFn_(defaultsFn) {
        if (!defaultsFn_) defaultsFn_ = &zeroDefaults;
        defaultsFn_(data_);
    }

    // прочитать из файла, проверить CRC и заголовок
    bool load() {
        valid_ = false;

        File f = fs_.open(path_, "r");
        if (!f) return false;
        if (f.size() < (int)sizeof(Header)) {
            f.close();
            return false;
        }

        Header hdr;
        if (f.read(reinterpret_cast<uint8_t*>(&hdr), sizeof(hdr)) != sizeof(hdr)) {
            f.close();
            return false;
        }

        if (hdr.magic != magic_ ||
            hdr.version != version_ ||
            hdr.dataSize != sizeof(T)) {
            f.close();
            return false;
        }

        uint8_t buf[sizeof(T)];
        if (f.read(buf, hdr.dataSize) != hdr.dataSize) {
            f.close();
            return false;
        }
        f.close();

        uint32_t calc = crc32(buf, hdr.dataSize);
        if (calc != hdr.crc) return false;

        memcpy(&data_, buf, sizeof(T));
        valid_ = true;
        return true;
    }

    // записать структуру в файл с заголовком и CRC
    bool save() const {
        Header hdr;
        hdr.magic    = magic_;
        hdr.version  = version_;
        hdr.dataSize = sizeof(T);
        hdr.crc      = crc32(&data_, sizeof(T));

        File f = fs_.open(path_, "w");
        if (!f) return false;

        bool ok = true;
        if (f.write(reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr)) != sizeof(hdr))
            ok = false;
        else if (f.write(reinterpret_cast<const uint8_t*>(&data_), sizeof(T)) != sizeof(T))
            ok = false;

        f.flush();
        f.close();
        return ok;
    }

    // загрузить или взять дефолты и сразу сохранить
    bool loadOrDefault() {
        if (load()) return true;
        resetToDefaults(false);
        bool ok = save();
        valid_ = ok;
        return ok;
    }

    // сбросить на дефолты; при autosave=true сразу сохранить
    void resetToDefaults(bool autosave = true) {
        if (defaultsFn_) defaultsFn_(data_);
        else zeroDefaults(data_);
        valid_ = false;
        if (autosave) {
            if (save()) valid_ = true;
        }
    }

    // доступ к данным
    T& data() { return data_; }
    const T& data() const { return data_; }

    bool isValid() const { return valid_; }

  private:
    static void zeroDefaults(T& t) {
        memset(&t, 0, sizeof(T));
    }

    static uint32_t crc32(const void* data, size_t len) {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        uint32_t crc = 0xFFFFFFFFu;
        while (len--) {
            crc ^= *p++;
            for (int i = 0; i < 8; ++i) {
                if (crc & 1)
                    crc = (crc >> 1) ^ 0xEDB88320u;
                else
                    crc >>= 1;
            }
        }
        return ~crc;
    }
};

} // namespace StructFS
