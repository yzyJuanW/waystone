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
    construct,
    copy_construct,
    move_construct,
    copy_assign,
    move_assign,
    destroy,
    constructor_body,
    caught,
};

struct Event {
    EventKind kind;
    std::string_view object;
    std::string_view related{};

    bool operator==(const Event&) const = default;
};

class EventLog {
public:
    void record(Event event) noexcept {
        if (size_ == events_.size()) {
            std::terminate();
        }
        events_[size_++] = event;
    }

    [[nodiscard]] std::span<const Event> events() const noexcept {
        return {events_.data(), size_};
    }

private:
    std::array<Event, 32> events_{};
    std::size_t size_{};
};

class Tracer {
public:
    Tracer(EventLog& log, std::string_view name) noexcept : log_(log), name_(name) {
        log_.record({EventKind::construct, name_});
    }

    Tracer(const Tracer& other, std::string_view name = "copy") noexcept
        : log_(other.log_), name_(name) {
        log_.record({EventKind::copy_construct, name_, other.name_});
    }

    Tracer(Tracer&& other, std::string_view name = "moved") noexcept
        : log_(other.log_), name_(name) {
        log_.record({EventKind::move_construct, name_, other.name_});
        other.moved_from_ = true;
    }

    Tracer& operator=(const Tracer& other) noexcept {
        log_.record({EventKind::copy_assign, name_, other.name_});
        moved_from_ = false;
        return *this;
    }

    Tracer& operator=(Tracer&& other) noexcept {
        log_.record({EventKind::move_assign, name_, other.name_});
        moved_from_ = false;
        other.moved_from_ = true;
        return *this;
    }

    ~Tracer() noexcept {
        log_.record({EventKind::destroy, name_, moved_from_ ? "moved-from" : ""});
    }

private:
    EventLog& log_;
    std::string_view name_;
    bool moved_from_{};
};

class FailingObject {
public:
    explicit FailingObject(EventLog& log) : member_(log, "member"), log_(log) {
        log_.record({EventKind::constructor_body, "complete-object"});
        throw std::runtime_error("construction failed");
    }

    ~FailingObject() noexcept {
        log_.record({EventKind::destroy, "complete-object"});
    }

private:
    Tracer member_;
    EventLog& log_;
};

constexpr std::string_view to_string(EventKind kind) noexcept {
    switch (kind) {
    case EventKind::construct: return "construct";
    case EventKind::copy_construct: return "copy-construct";
    case EventKind::move_construct: return "move-construct";
    case EventKind::copy_assign: return "copy-assign";
    case EventKind::move_assign: return "move-assign";
    case EventKind::destroy: return "destroy";
    case EventKind::constructor_body: return "constructor-body";
    case EventKind::caught: return "caught";
    }
    std::terminate();
}

void print(std::string_view scenario, std::span<const Event> events) {
    std::cout << "\n[" << scenario << "]\n";
    for (const auto& event : events) {
        std::cout << "  " << to_string(event.kind) << ": " << event.object;
        if (!event.related.empty()) {
            std::cout << " <- " << event.related;
        }
        std::cout << '\n';
    }
}

void verify(std::string_view scenario,
            std::span<const Event> actual,
            std::span<const Event> expected) {
    print(scenario, actual);
    if (actual.size() != expected.size()) {
        throw std::runtime_error("unexpected event count in " + std::string(scenario));
    }
    for (std::size_t index = 0; index < actual.size(); ++index) {
        if (actual[index] != expected[index]) {
            throw std::runtime_error("unexpected event in " + std::string(scenario));
        }
    }
}

void scope_scenario() {
    EventLog log;
    {
        Tracer first(log, "first");
        Tracer second(log, "second");
    }

    constexpr std::array expected{
        Event{EventKind::construct, "first"},
        Event{EventKind::construct, "second"},
        Event{EventKind::destroy, "second"},
        Event{EventKind::destroy, "first"},
    };
    verify("scope: reverse destruction order", log.events(), expected);
}

void copy_move_scenario() {
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

    constexpr std::array expected{
        Event{EventKind::construct, "source"},
        Event{EventKind::copy_construct, "copied", "source"},
        Event{EventKind::move_construct, "moved", "source"},
        Event{EventKind::construct, "copy-target"},
        Event{EventKind::copy_assign, "copy-target", "copied"},
        Event{EventKind::construct, "move-target"},
        Event{EventKind::move_assign, "move-target", "copied"},
        Event{EventKind::destroy, "move-target"},
        Event{EventKind::destroy, "copy-target"},
        Event{EventKind::destroy, "moved"},
        Event{EventKind::destroy, "copied", "moved-from"},
        Event{EventKind::destroy, "source", "moved-from"},
    };
    verify("explicit copy and move operations", log.events(), expected);
}

void stack_unwinding_scenario() {
    EventLog log;
    try {
        Tracer first(log, "first-local");
        Tracer second(log, "second-local");
        throw std::runtime_error("leave scope");
    } catch (const std::runtime_error&) {
        log.record({EventKind::caught, "stack-unwinding"});
    }

    constexpr std::array expected{
        Event{EventKind::construct, "first-local"},
        Event{EventKind::construct, "second-local"},
        Event{EventKind::destroy, "second-local"},
        Event{EventKind::destroy, "first-local"},
        Event{EventKind::caught, "stack-unwinding"},
    };
    verify("exception: stack unwinding", log.events(), expected);
}

void failed_construction_scenario() {
    EventLog log;
    try {
        FailingObject object(log);
    } catch (const std::runtime_error&) {
        log.record({EventKind::caught, "construction-failure"});
    }

    constexpr std::array expected{
        Event{EventKind::construct, "member"},
        Event{EventKind::constructor_body, "complete-object"},
        Event{EventKind::destroy, "member"},
        Event{EventKind::caught, "construction-failure"},
    };
    verify("exception: incomplete object", log.events(), expected);
}

int main() {
    try {
        scope_scenario();
        copy_move_scenario();
        stack_unwinding_scenario();
        failed_construction_scenario();
        std::cout << "\nAll object lifetime checks passed.\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "object_lifetime_lab failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
