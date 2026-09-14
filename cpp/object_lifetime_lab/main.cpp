#include <array>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

enum class EventKind {
  kConstruct,
  kCopyConstruct,
  kMoveConstruct,
  kCopyAssign,
  kMoveAssign,
  kDestroy,
  kConstructorBody,
  kCaught,
};

struct Event {
  EventKind kind;
  std::string_view object;
  std::string_view related{};

  bool operator==(const Event&) const = default;
};

class EventLog {
 public:
  void Record(Event event) noexcept {
    if (size_ == events_.size()) std::terminate();
    events_[size_++] = event;
  }

  [[nodiscard]] std::span<const Event> Events() const noexcept {
    return {events_.data(), size_};
  }

 private:
  std::array<Event, 32> events_{};
  std::size_t size_{};
};

class Tracer {
 public:
  Tracer(EventLog& log, std::string_view name) noexcept : log_(log), name_(name) {
    log_.Record({EventKind::kConstruct, name_});
  }

  Tracer(const Tracer& other, std::string_view name = "copy") noexcept
      : log_(other.log_), name_(name) {
    log_.Record({EventKind::kCopyConstruct, name_, other.name_});
  }

  Tracer(Tracer&& other, std::string_view name = "moved") noexcept
      : log_(other.log_), name_(name) {
    log_.Record({EventKind::kMoveConstruct, name_, other.name_});
    other.moved_from_ = true;
  }

  Tracer& operator=(const Tracer& other) noexcept {
    log_.Record({EventKind::kCopyAssign, name_, other.name_});
    moved_from_ = false;
    return *this;
  }

  Tracer& operator=(Tracer&& other) noexcept {
    log_.Record({EventKind::kMoveAssign, name_, other.name_});
    moved_from_ = false;
    other.moved_from_ = true;
    return *this;
  }

  ~Tracer() noexcept {
    log_.Record({EventKind::kDestroy, name_, moved_from_ ? "moved-from" : ""});
  }

 private:
  EventLog& log_;
  std::string_view name_;
  bool moved_from_{};
};

class FailingObject {
 public:
  explicit FailingObject(EventLog& log) : member_(log, "member"), log_(log) {
    log_.Record({EventKind::kConstructorBody, "complete-object"});
    throw std::runtime_error("construction failed");
  }

  ~FailingObject() noexcept { log_.Record({EventKind::kDestroy, "complete-object"}); }

 private:
  Tracer member_;
  EventLog& log_;
};

constexpr std::string_view ToString(EventKind kind) noexcept {
  switch (kind) {
    case EventKind::kConstruct:
      return "construct";
    case EventKind::kCopyConstruct:
      return "copy-construct";
    case EventKind::kMoveConstruct:
      return "move-construct";
    case EventKind::kCopyAssign:
      return "copy-assign";
    case EventKind::kMoveAssign:
      return "move-assign";
    case EventKind::kDestroy:
      return "destroy";
    case EventKind::kConstructorBody:
      return "constructor-body";
    case EventKind::kCaught:
      return "caught";
  }
  std::terminate();
}

void Print(std::string_view scenario, std::span<const Event> events) {
  std::cout << "\n[" << scenario << "]\n";
  for (const auto& event : events) {
    std::cout << "  " << ToString(event.kind) << ": " << event.object;
    if (!event.related.empty()) std::cout << " <- " << event.related;
    std::cout << '\n';
  }
}

void Verify(std::string_view scenario, std::span<const Event> actual,
            std::span<const Event> expected) {
  Print(scenario, actual);
  if (actual.size() != expected.size()) {
    throw std::runtime_error("unexpected event count in " + std::string(scenario));
  }
  for (std::size_t index = 0; index < actual.size(); ++index) {
    if (actual[index] != expected[index]) {
      throw std::runtime_error("unexpected event in " + std::string(scenario));
    }
  }
}

void ScopeScenario() {
  EventLog log;
  {
    Tracer first(log, "first");
    Tracer second(log, "second");
  }

  constexpr std::array kExpected{
      Event{EventKind::kConstruct, "first"},
      Event{EventKind::kConstruct, "second"},
      Event{EventKind::kDestroy, "second"},
      Event{EventKind::kDestroy, "first"},
  };
  Verify("scope: reverse destruction order", log.Events(), kExpected);
}

void CopyMoveScenario() {
  EventLog log;
  {
    Tracer source(log, "source");
    Tracer copied(source, "copied");
    Tracer moved(std::move(source), "moved");
    Tracer copy_target(log, "copy-target");
    copy_target = copied;
    Tracer move_target(log, "move-target");
    move_target = std::move(copied);
  }

  constexpr std::array kExpected{
      Event{EventKind::kConstruct, "source"},
      Event{EventKind::kCopyConstruct, "copied", "source"},
      Event{EventKind::kMoveConstruct, "moved", "source"},
      Event{EventKind::kConstruct, "copy-target"},
      Event{EventKind::kCopyAssign, "copy-target", "copied"},
      Event{EventKind::kConstruct, "move-target"},
      Event{EventKind::kMoveAssign, "move-target", "copied"},
      Event{EventKind::kDestroy, "move-target"},
      Event{EventKind::kDestroy, "copy-target"},
      Event{EventKind::kDestroy, "moved"},
      Event{EventKind::kDestroy, "copied", "moved-from"},
      Event{EventKind::kDestroy, "source", "moved-from"},
  };
  Verify("explicit copy and move operations", log.Events(), kExpected);
}

void StackUnwindingScenario() {
  EventLog log;
  try {
    Tracer first(log, "first-local");
    Tracer second(log, "second-local");
    throw std::runtime_error("leave scope");
  } catch (const std::runtime_error&) {
    log.Record({EventKind::kCaught, "stack-unwinding"});
  }

  constexpr std::array kExpected{
      Event{EventKind::kConstruct, "first-local"},
      Event{EventKind::kConstruct, "second-local"},
      Event{EventKind::kDestroy, "second-local"},
      Event{EventKind::kDestroy, "first-local"},
      Event{EventKind::kCaught, "stack-unwinding"},
  };
  Verify("exception: stack unwinding", log.Events(), kExpected);
}

void FailedConstructionScenario() {
  EventLog log;
  try {
    FailingObject object(log);
  } catch (const std::runtime_error&) {
    log.Record({EventKind::kCaught, "construction-failure"});
  }

  constexpr std::array kExpected{
      Event{EventKind::kConstruct, "member"},
      Event{EventKind::kConstructorBody, "complete-object"},
      Event{EventKind::kDestroy, "member"},
      Event{EventKind::kCaught, "construction-failure"},
  };
  Verify("exception: incomplete object", log.Events(), kExpected);
}

int main() {
  try {
    ScopeScenario();
    CopyMoveScenario();
    StackUnwindingScenario();
    FailedConstructionScenario();
    std::cout << "\nAll object lifetime checks passed.\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "object_lifetime_lab failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
