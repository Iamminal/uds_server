#include "isotpreassembler.h"

std::optional<QByteArray> IsoTpReassembler::ingest(quint32 canId, const QByteArray& frame) {
    if (frame.isEmpty()) return std::nullopt;
    const quint8 pci = static_cast<quint8>(frame.at(0));
    const quint8 type = pci >> 4;
    if (type == 0) {
        const int length = pci & 0x0F;
        if (length < 1 || frame.size() < length + 1) return std::nullopt;
        states_.remove(canId);
        return frame.mid(1, length);
    }
    if (type == 1 && frame.size() >= 8) {
        const int expected = ((pci & 0x0F) << 8) | static_cast<quint8>(frame.at(1));
        if (expected <= 7 || expected > 4095) return std::nullopt;
        states_.insert(canId, State{frame.mid(2), expected, 1});
        return std::nullopt;
    }
    if (type == 2 && states_.contains(canId)) {
        auto& state = states_[canId];
        if ((pci & 0x0F) != state.sequence) {
            states_.remove(canId);
            return std::nullopt;
        }
        state.sequence = (state.sequence + 1) & 0x0F;
        state.data.append(frame.mid(1));
        if (state.data.size() >= state.expected) {
            const QByteArray complete = state.data.left(state.expected);
            states_.remove(canId);
            return complete;
        }
    }
    return std::nullopt;
}

void IsoTpReassembler::reset(quint32 canId) { states_.remove(canId); }
void IsoTpReassembler::clear() { states_.clear(); }
