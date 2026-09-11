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

#include "rewindmanager.h"

#include "log.h"

namespace libretrodroid {

RewindManager::RewindManager(size_t memoryLimitBytes, unsigned int captureIntervalFrames):
    memoryLimitBytes(memoryLimitBytes),
    captureIntervalFrames(captureIntervalFrames > 0 ? captureIntervalFrames : 1) {
}

void RewindManager::init(size_t newStateSize) {
    enabled = false;
    buffer.clear();
    buffer.shrink_to_fit();
    stateSize = 0;
    slotCount = 0;
    writeIndex = 0;
    filledSlots = 0;
    framesSinceLastCapture = 0;

    if (newStateSize == 0) {
        LOGW("RewindManager: core reports a serialize size of 0. Rewind disabled.");
        return;
    }

    if (newStateSize > memoryLimitBytes) {
        LOGW(
            "RewindManager: state size (%zu bytes) exceeds the memory budget (%zu bytes). Rewind disabled.",
            newStateSize,
            memoryLimitBytes
        );
        return;
    }

    // At least two slots are required for rewind to have any visible effect
    // (one state to unserialize into, plus the one behind it). This can push
    // the ring's total size slightly above memoryLimitBytes for very large
    // states, which is an accepted tradeoff to keep rewind functional.
    size_t computedSlotCount = memoryLimitBytes / newStateSize;
    if (computedSlotCount < 2) {
        computedSlotCount = 2;
    }

    // Preallocate the whole ring up front. No per-capture allocation happens afterwards.
    buffer.assign(computedSlotCount * newStateSize, 0);

    stateSize = newStateSize;
    slotCount = computedSlotCount;
    enabled = true;

    LOGI(
        "RewindManager: initialized with %zu slots of %zu bytes each (%zu bytes total).",
        slotCount,
        stateSize,
        buffer.size()
    );
}

void RewindManager::onFrame(Core* core) {
    if (!enabled || core == nullptr) {
        return;
    }

    framesSinceLastCapture++;
    if (framesSinceLastCapture < captureIntervalFrames) {
        return;
    }
    framesSinceLastCapture = 0;

    int8_t* slot = buffer.data() + writeIndex * stateSize;
    if (!core->retro_serialize(slot, stateSize)) {
        LOGE("RewindManager: retro_serialize failed while capturing a rewind state.");
        return;
    }

    writeIndex = (writeIndex + 1) % slotCount;
    if (filledSlots < slotCount) {
        filledSlots++;
    }
}

bool RewindManager::rewindStep(Core* core) {
    if (!enabled || core == nullptr || filledSlots == 0) {
        return false;
    }

    filledSlots--;
    writeIndex = (writeIndex + slotCount - 1) % slotCount;

    int8_t* slot = buffer.data() + writeIndex * stateSize;
    if (!core->retro_unserialize(slot, stateSize)) {
        LOGE("RewindManager: retro_unserialize failed while rewinding.");
    }

    return true;
}

void RewindManager::reset() {
    writeIndex = 0;
    filledSlots = 0;
    framesSinceLastCapture = 0;
}

bool RewindManager::isEnabled() const {
    return enabled;
}

} //namespace libretrodroid
