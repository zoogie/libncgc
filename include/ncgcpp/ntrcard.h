/* libncgc
 * Copyright (C) 2017 angelsl
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NCGCPP_NTRCARD_H
#define NCGCPP_NTRCARD_H

#if !defined(__cplusplus)
#error This file should not be included from C.
#endif

#if defined(NCGC_PLATFORM_NTR)
#elif defined(NCGC_PLATFORM_CTR)
#else
    // FIXME no C++ tests yet
    #error No NCGC platform defined.
#endif

#include <cstdint>
#include <cstddef>

namespace ncgc {
namespace c {
#define NCGC_CPP_WRAPPER
extern "C" {
    #include "../ncgc/ntrcard.h"
}
#undef NCGC_CPP_WRAPPER
}
}

namespace ncgc {
inline void delay(std::uint32_t delay) {
#if defined(NCGC_PLATFORM_NTR)
    ncgc_platform_ntr_delay(delay);
#elif defined(NCGC_PLATFORM_CTR)
    ncgc_platform_ctr_delay(delay);
#endif
}

enum class NTRState {
    Raw,
    Key1,
    Key2,
    Unknown
};

class NTRCard;
class NTRFlags {
    /// The raw ROMCNT value.
    std::uint32_t romcnt;
    friend NTRCard;
public:
    constexpr bool bit(uint32_t bit) const { return !!(romcnt & (1 << bit)); }
    constexpr NTRFlags bit(uint32_t bit, bool set) { return (romcnt & ~(1 << bit)) | (set ? (1 << bit) : 0); }

    /// Returns the delay before the response to a KEY1 command (KEY1 gap1)
    constexpr std::uint16_t preDelay() const { return static_cast<uint16_t>(romcnt & 0x1FFF); }
    /// Returns the delay after the response to a KEY1 command (KEY1 gap2)
    constexpr std::uint16_t postDelay() const { return static_cast<uint16_t>((romcnt >> 16) & 0x3F); }
    /// Returns true if clock pulses should be sent, and the KEY2 state advanced, during the pre- and post(?)-delays
    constexpr bool delayPulseClock() const { return bit(28); }
    /// Returns true if the command is KEY2-encrypted
    constexpr bool key2Command() const { return bit(22) && bit(14); }
    /// Returns true if the command is KEY2-encrypted
    constexpr bool key2Response() const { return bit(13) && bit(14); }
    /// Returns true if the slower CLK rate should be used (usually for raw commands)
    constexpr bool slowClock() const { return bit(27); }

    /// Sets the the delay before the response to a KEY1 command (KEY1 gap1)
    constexpr NTRFlags preDelay(std::uint16_t value) { return (romcnt & ~0x1FFF) | (value & 0x1FFF); }
    /// Sets the delay after the response to a KEY1 command (KEY1 gap2)
    constexpr NTRFlags postDelay(std::uint16_t value) { return (romcnt & ~(0x3F << 16)) | ((value & 0x3F) << 16); }
    /// Set if clock pulses should be sent, and the KEY2 state advanced, during the pre- and post(?)-delays
    constexpr NTRFlags delayPulseClock(bool value) { return bit(28, value); }
    /// Set if the command is KEY2-encrypted
    constexpr NTRFlags key2Command(bool value) { return bit(22, value).bit(14, value || bit(13)); }
    /// Set if the command is KEY2-encrypted
    constexpr NTRFlags key2Response(bool value) { return bit(13, value).bit(14, value || bit(22)); }
    /// Set if the slower CLK rate should be used (usually for raw commands)
    constexpr NTRFlags slowClock(bool value) { return bit(27, value); }

    constexpr operator std::uint32_t() const { return romcnt; }
    constexpr NTRFlags(const std::uint32_t& from) : romcnt(from) {}
};

class NTRCard {
#if defined(NCGC_PLATFORM_NTR)
    template<typename ResetFn>
    inline NTRCard(ResetFn f) {
        ncgc_nplatform_ntr_init(&_card, f);
    }
#elif defined(NCGC_PLATFORM_CTR)
    inline NTRCard() {
        ncgc_nplatform_ctr_init(&_card);
    }

    inline static void waitForCard() {
        ncgc_nplatform_ctr_wait_for_card();
    }

    inline static bool cardInserted() {
        return ncgc_nplatform_ctr_card_inserted();
    }
#endif

    // you cannot copy this class, it does not make sense
    NTRCard(const NTRCard& other) = delete;
    NTRCard& operator=(const NTRCard& other) = delete;

    inline __ncgc_must_check std::int32_t init(void *buffer = nullptr, bool header_first = false) {
        return c::ncgc_ninit_order(&_card, buffer, header_first);
    }

    inline __ncgc_must_check std::int32_t beginKey1() {
        return c::ncgc_nbegin_key1(&_card);
    }

    inline __ncgc_must_check std::int32_t beginKey2() {
        return c::ncgc_nbegin_key2(&_card);
    }

    inline void setBlowfishState(const std::uint32_t (&ps)[NCGC_NBF_PS_N32], bool as_is = false) {
        if (as_is) {
            c::ncgc_nsetup_blowfish_as_is(&_card, ps);
        } else {
            c::ncgc_nsetup_blowfish(&_card, ps);
        }
    }

    inline __ncgc_must_check std::int32_t readData(const std::uint32_t address, void *const buf, const std::size_t size) {
        return c::ncgc_nread_data(&_card, address, buf, size);
    }

    inline __ncgc_must_check std::int32_t readSecureArea(void *buffer) {
        return c::ncgc_nread_secure_area(&_card, buffer);
    }

    inline __ncgc_must_check std::int32_t sendCommand(const uint64_t command, void *const buf, const size_t size, NTRFlags flags, bool flagsAsIs = false) {
        if (flagsAsIs) {
            return ncgc_nsend_command_as_is(&_card, command, buf, size, (c::ncgc_nflags_t) { static_cast<uint32_t>(flags) });
        } else {
            return ncgc_nsend_command(&_card, command, buf, size, (c::ncgc_nflags_t) { static_cast<uint32_t>(flags) });
        }
    }

    inline NTRState state() {
        return static_cast<NTRState>(_card.encryption_state);
    }

    inline std::uint32_t gameCode() {
        return _card.hdr.game_code;
    }

    inline std::uint32_t chipId() {
        return _card.raw_chipid;
    }

    inline NTRFlags key1Flags() {
        return _card.hdr.key1_romcnt;
    }

    inline NTRFlags key2Flags() {
        return _card.hdr.key2_romcnt;
    }

    inline c::ncgc_ncard_t& rawState() {
        return _card;
    }
private:
    c::ncgc_ncard_t _card;
};
}
#endif /* NCGCPP_NTRCARD_H */