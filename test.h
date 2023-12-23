#pragma once

#include <memory>
#include <vector>
#include <string>
#include <exception>
#include <map>
#include <cstring>
#include <unordered_set>
#include <iostream>
#include <map>

class AbstractTest {
public:
    virtual void SetUp() = 0;
    virtual void TearDown() = 0;
    virtual void Run() = 0;
    virtual ~AbstractTest() = default;
};

class AbstractCreator {
public:
    AbstractCreator() = default;
    virtual std::unique_ptr<AbstractTest> Create() const = 0;
    virtual ~AbstractCreator(){};
};

template <class Test>
class ConcreteCreator : public AbstractCreator {
public:
    std::unique_ptr<AbstractTest> Create() const override {
        return std::make_unique<Test>();
    }
    ~ConcreteCreator(){};
    ConcreteCreator(const std::type_info* inf) : info_(inf) {
    }

private:
    const std::type_info* info_;
};

class TestRegistry {
private:
    TestRegistry() = default;
    ~TestRegistry() = default;

    TestRegistry(const TestRegistry&) = delete;
    TestRegistry& operator=(const TestRegistry&) = delete;

    TestRegistry& operator=(const TestRegistry&&) = delete;
    TestRegistry(const TestRegistry&&) = delete;

    std::map<std::string, std::unique_ptr<AbstractCreator>> factory_;
    std::map<std::string, std::string> info_;

public:
    static TestRegistry& Instance() {
        static TestRegistry instance;
        return instance;
    };

    template <class TestClass>
    void RegisterClass(const std::string& class_name) {
        std::string current_type = typeid(TestClass).name();
        // std::cout << current_type << '\n';
        for (auto& it : info_) {
            // std::cout << it.second << '\n';
            if (it.second == current_type) {
                // std::cout << "same types \n";
                throw std::runtime_error("we already have test with such name or type");
            }
        }
        auto iterator_name = factory_.find(class_name);
        if (iterator_name == factory_.end()) {
            // std::cout << "insert new value \n";
            factory_.insert(
                {class_name, std::make_unique<ConcreteCreator<TestClass>>(&typeid(TestClass))});
            info_.insert({class_name, current_type});
        } else {
            throw std::runtime_error("we already have test with such name or type");
        }
    }

    std::unique_ptr<AbstractTest> CreateTest(const std::string& class_name) const {
        auto it = factory_.find(class_name);
        if (it != factory_.end()) {
            return it->second->Create();
        } else {
            throw std::out_of_range("there is no such element in map");
        }
    }

    void RunTest(const std::string& test_name) {
        auto test = CreateTest(test_name);
        test->SetUp();
        try {
            test->Run();
        } catch (...) {
            test->TearDown();
            throw;
        }
        test->TearDown();
    }

    void Clear() {
        factory_.clear();
        info_.clear();
    }

    std::vector<std::string> ShowAllTests() {
        std::vector<std::string> result;
        for (auto& item : factory_) {
            result.push_back(item.first);
        }
        return result;
    }

    template <class Predicate>
    void RunTests([[maybe_unused]] Predicate callback) {
        // std::cout << "start run tests \n";
        for (auto& it : factory_) {
            // std::cout << "new iteration! \n";
            if (callback(it.first)) {
                RunTest(it.first);
            }
        }
    }

    template <class Predicate>
    std::vector<std::string> ShowTests(Predicate callback) {
        std::vector<std::string> result;
        for (auto& it : factory_) {
            if (callback(it.first)) {
                result.push_back(it.first);
            }
        }
        return result;
    }
};

class FullMatch {
public:
    FullMatch(std::string str) : data_(str){};
    bool operator()(std::string str_to_compare) {
        if (str_to_compare == data_) {
            return true;
        }
        return false;
    };

private:
    std::string_view data_;
};

class Substr {
public:
    Substr(const std::string str) : data_(str){};
    bool operator()(const std::string str) {
        for (size_t ind = 0; ind < str.size(); ++ind) {
            size_t cur_ind = ind;
            size_t is = 0;
            while (str[cur_ind] == data_[is]) {
                ++is;
                ++cur_ind;
                if (is == data_.size()) {
                    return true;
                }
            }
        }
        return false;
    }

private:
    const std::string data_;
};
