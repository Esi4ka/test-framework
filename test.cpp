#include "test.h"

#include <stdexcept>
#include <vector>
#include <concepts>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>

using Catch::Matchers::Equals;
using Catch::Matchers::RangeEquals;

struct Event {
    enum Method { kCreate, kSetup, kRun, kTeardown };

    Method method;
    std::string test;

    bool operator==(const Event&) const = default;
};

std::vector<Event> events;

struct RegistryTest {
    RegistryTest() {
        events.clear();
        registry.Clear();
    }

    void RequireTests(std::initializer_list<std::string_view> list) {
        REQUIRE_THAT(registry.ShowAllTests(), RangeEquals(list));
    }

    void RequireTests(std::initializer_list<std::string_view> list, const auto& pred) {
        REQUIRE_THAT(registry.ShowTests(pred), RangeEquals(list));
    }

    void RequireEvents(const std::vector<Event>& expected) {
        REQUIRE_THAT(events, Equals(expected));
    }

    TestRegistry& registry = TestRegistry::Instance();
};

struct SimpleTest : AbstractTest {
    void SetUp() override {
        events.emplace_back(Event::kSetup, "simple_test");
    }
    void TearDown() override {
        events.emplace_back(Event::kTeardown, "simple_test");
    }
};

struct FirstTest : SimpleTest {
    FirstTest() {
        events.emplace_back(Event::kCreate, "first_test");
    }
    void Run() override {
        events.emplace_back(Event::kRun, "first_test");
    }
};

struct SecondTest : SimpleTest {
    SecondTest() {
        events.emplace_back(Event::kCreate, "second_test");
    }

    SecondTest(const SecondTest&) = delete;
    SecondTest(SecondTest&&) = delete;
    SecondTest& operator=(const SecondTest&) = delete;
    SecondTest& operator=(SecondTest&&) = delete;

    void Run() override {
        events.emplace_back(Event::kRun, "second_test");
    }

    void TearDown() override {
        events.emplace_back(Event::kTeardown, "second_test");
    }
};

struct ThirdTest : AbstractTest {
    void Run() override {
        throw std::runtime_error{"shit happens"};
    }

    void SetUp() override {
        events.emplace_back(Event::kSetup, "third_test");
    }

    void TearDown() override {
        events.emplace_back(Event::kTeardown, "third_test");
    }
};

TEST_CASE_METHOD(RegistryTest, "Registry is singleton") {
    STATIC_CHECK_FALSE(std::default_initializable<TestRegistry>);
    STATIC_CHECK_FALSE(std::copy_constructible<TestRegistry>);
    STATIC_CHECK_FALSE(std::move_constructible<TestRegistry>);
    STATIC_CHECK_FALSE(std::assignable_from<TestRegistry&, const TestRegistry&>);
    STATIC_CHECK_FALSE(std::assignable_from<TestRegistry&, TestRegistry&&>);

    registry.RegisterClass<FirstTest>("first_test");
    registry.RegisterClass<SecondTest>("second_test");

    auto& registry2 = TestRegistry::Instance();
    REQUIRE(&registry == &registry2);
    RequireTests({"first_test", "second_test"});
    REQUIRE(events.empty());
}

TEST_CASE_METHOD(RegistryTest, "Registry doesn't run tests") {
    registry.RegisterClass<SecondTest>("check");
    RequireTests({"check"});
    REQUIRE(events.empty());
}

TEST_CASE_METHOD(RegistryTest, "Show filter of registry") {
    registry.RegisterClass<FirstTest>("aba");
    registry.RegisterClass<SecondTest>("hello");
    registry.RegisterClass<ThirdTest>("caba");
    RequireTests({"aba", "caba"}, Substr{"aba"});
    RequireTests({"aba"}, FullMatch{"aba"});
    RequireTests({}, FullMatch{"hell"});
}

TEST_CASE_METHOD(RegistryTest, "Custom predicate") {
    constexpr auto kIsEvenLength = [](const std::string& s) { return s.size() % 2 == 0; };
    registry.RegisterClass<FirstTest>("aba");
    registry.RegisterClass<SecondTest>("caba");
    registry.RegisterClass<ThirdTest>("abacaba");
    RequireTests({"caba"}, kIsEvenLength);
    registry.RunTests(kIsEvenLength);
    RequireEvents({{Event::kCreate, "second_test"},
                   {Event::kSetup, "simple_test"},
                   {Event::kRun, "second_test"},
                   {Event::kTeardown, "second_test"}});
}

TEST_CASE_METHOD(RegistryTest, "Single run") {
    registry.RegisterClass<FirstTest>("first_test");
    registry.RunTest("first_test");
    RequireEvents({{Event::kCreate, "first_test"},
                   {Event::kSetup, "simple_test"},
                   {Event::kRun, "first_test"},
                   {Event::kTeardown, "simple_test"}});
}

struct AnotherTest : SimpleTest {
    AnotherTest() {
        counter = 0;
    }
    void Run() override {
        ++counter;
    }
    static inline auto counter = 0;
};

TEST_CASE_METHOD(RegistryTest, "No test creation in RegistryClass") {
    AnotherTest::counter = 42;
    registry.RegisterClass<AnotherTest>("test");
    REQUIRE(AnotherTest::counter == 42);
    registry.CreateTest("test");
    REQUIRE(AnotherTest::counter == 0);
    REQUIRE(events.empty());
}

TEST_CASE_METHOD(RegistryTest, "Create tests") {
    registry.RegisterClass<SecondTest>("second_test");
    registry.RegisterClass<ThirdTest>("third_test");
    registry.RegisterClass<AnotherTest>("check");
    auto test = registry.CreateTest("third_test");
    test->SetUp();

    REQUIRE_THROWS_AS(test->Run(), std::runtime_error);
    REQUIRE_THROWS_AS(registry.CreateTest("first_test"), std::out_of_range);

    auto first_test = registry.CreateTest("check");
    first_test->Run();
    REQUIRE(AnotherTest::counter == 1);
    auto second_test = registry.CreateTest("check");
    second_test->Run();
    REQUIRE(AnotherTest::counter == 1);
}

TEST_CASE_METHOD(RegistryTest, "Run all") {
    registry.Clear();
    registry.RegisterClass<ThirdTest>("third_test");
    registry.RegisterClass<FirstTest>("first_test");
    registry.RegisterClass<SecondTest>("second_test");
    auto f = [](const std::string&) { return true; };
    REQUIRE_THROWS_WITH(registry.RunTests(f), "shit happens");
    RequireEvents({{Event::kCreate, "first_test"},
                   {Event::kSetup, "simple_test"},
                   {Event::kRun, "first_test"},
                   {Event::kTeardown, "simple_test"},
                   {Event::kCreate, "second_test"},
                   {Event::kSetup, "simple_test"},
                   {Event::kRun, "second_test"},
                   {Event::kTeardown, "second_test"},
                   {Event::kSetup, "third_test"},
                   {Event::kTeardown, "third_test"}});
}

TEST_CASE_METHOD(RegistryTest, "Same name exception") {
    registry.RegisterClass<FirstTest>("test");
    REQUIRE_THROWS_AS(registry.RegisterClass<SecondTest>("test"), std::runtime_error);
    REQUIRE(dynamic_cast<FirstTest*>(registry.CreateTest("test").get()));
}

TEST_CASE_METHOD(RegistryTest, "Same type exception") {
    registry.RegisterClass<FirstTest>("test");
    REQUIRE_THROWS_AS(registry.RegisterClass<FirstTest>("test2"), std::runtime_error);
    registry.RegisterClass<SecondTest>("test2");
    REQUIRE(dynamic_cast<SecondTest*>(registry.CreateTest("test2").get()));
    REQUIRE_THROWS_AS(registry.RegisterClass<SecondTest>("test3"), std::runtime_error);
    REQUIRE_THROWS_AS(registry.RegisterClass<SecondTest>("test2"), std::runtime_error);
}
