/*
 *     Copyright (C) 2024  Filippo Scognamiglio
 *
 *     This program is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef LIBRETRODROID_REWINDMANAGER_H
#define LIBRETRODROID_REWINDMANAGER_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core.h"

namespace libretrodroid {

// Hold-to-rewind engine backed by a single preallocated ring buffer of raw
// retro_serialize() snapshots. There is no compression in this iteration: the
// buffer is sized as (memoryLimitBytes / stateSize) slots, each holding one
// uncompressed state. A future iteration can add a compression layer between
// onFrame()/rewindStep() and the ring storage without changing this class'
// public contract.
class RewindManager {
public:
    RewindManager(size_t memoryLimitBytes, unsigned int captureIntervalFrames);

    // Prepares the ring buffer once the core's serialization size is known
    // (typically right after a game is loaded). Disables rewind cleanly (no
    // allocation, isEnabled() returns false) when stateSize is zero or does
    // not fit within the configured memory budget.
    void init(size_t stateSize);

    // Called once per emulated frame, right after retro_run(). Captures a
    // snapshot every captureIntervalFrames frames, overwriting the oldest
    // slot once the ring is full. No-op when rewind is disabled.
    void onFrame(Core* core);

    // Pops the newest buffered state and applies it via retro_unserialize().
    // Consecutive calls walk backward through history. Returns false when
    // the buffer is empty; the caller should then simply keep rendering the
    // oldest state that is already applied.
    bool rewindStep(Core* core);

    // Clears the ring (indices only, the preallocated buffer is kept) so
    // stale states never replay across a game load/reset/user save load.
    void reset();

    bool isEnabled() const;

private:
    size_t memoryLimitBytes;
    unsigned int captureIntervalFrames;

    std::vector<int8_t> buffer;
    size_t stateSize = 0;
    size_t slotCount = 0;

    size_t writeIndex = 0;
    size_t filledSlots = 0;
    unsigned int framesSinceLastCapture = 0;

    bool enabled = false;
};

} //namespace libretrodroid

#endif //LIBRETRODROID_REWINDMANAGER_H
