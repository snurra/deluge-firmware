#pragma once

#include "rosic_MidiNoteEvent.h"
#include <array>
#include <cstddef>

namespace rosic {
// Last-note priority, with at most one entry per MIDI key and no heap allocation.
class NoteStack {
public:
  bool empty() const { return size_ == 0; }
  void clear() { size_ = 0; }
  const MidiNoteEvent& front() const { return notes_[0]; }
  void remove(const MidiNoteEvent& note) {
    for (std::size_t i = 0; i < size_; ++i) {
      if (notes_[i] == note) {
        for (std::size_t j = i + 1; j < size_; ++j) notes_[j - 1] = notes_[j];
        --size_;
        return;
      }
    }
  }
  void push_front(const MidiNoteEvent& note) {
    remove(note);
    if (size_ == notes_.size()) --size_;
    for (std::size_t i = size_; i > 0; --i) notes_[i] = notes_[i - 1];
    notes_[0] = note;
    ++size_;
  }
private:
  std::array<MidiNoteEvent, 128> notes_{};
  std::size_t size_ = 0;
};
}
