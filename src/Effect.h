#include <optional>

enum class EffectKind {
  Unchanged,
  Increment,
  Decrement,
  Multiply,
  UnknownChanged
};

struct EffectOnSubscript {
  EffectKind kind;
  std::optional<uint64_t> c;
};