#ifndef DFTRACER_AGGREGATOR_RULES_H
#define DFTRACER_AGGREGATOR_RULES_H
// internal headers
#include <dftracer/core/common/datastructure.h>

// standard headers
#include <algorithm>
#include <cctype>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace dftracer {

class Rules {
 public:
  Rules() = default;

  // Add a rule (as string), parse and build AST
  inline void addRule(const std::string& rule) {
    RuleAST ast = parseRule(rule);
    asts_.emplace_back(std::move(ast));
  }

  inline bool satisfies(const AggregatedKey* key) const {
    for (const auto& ast : asts_) {
      if (eval(ast.root.get(), key)) return true;
    }
    return false;
  }

 private:
  std::vector<RuleAST> asts_;

  inline std::string fixString(const std::string& str) const {
    std::string val = str;
    // Remove quotes if present
    if (!val.empty() && ((val.front() == '\'' && val.back() == '\'') ||
                         (val.front() == '"' && val.back() == '"'))) {
      val = val.substr(1, val.size() - 2);
    }
    return val;
  }
  inline Value stringToValue(const std::string& str) const {
    std::string val = str;
    // Remove quotes if present
    if (!val.empty() && ((val.front() == '\'' && val.back() == '\'') ||
                         (val.front() == '"' && val.back() == '"'))) {
      val = val.substr(1, val.size() - 2);
    }
    try {
      if (val.find('.') != std::string::npos) {
        return Value{std::stod(val)};
      } else {
        return Value{std::stoull(val)};
      }
    } catch (...) {
      return Value{val};
    }
  }
  RuleAST parseRule(const std::string& rule);

  bool eval(const RuleAST::Node* node, const AggregatedKey* key) const;
};

}  // namespace dftracer

#endif  // DFTRACER_AGGREGATOR_RULES_H