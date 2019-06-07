#include "gtest/gtest.h"
#include "rapidjson/document.h"
#include "mustache.h"

#include <vector>

using namespace rapidjson;
using namespace std;
using namespace mustache;

namespace mustache {

void FindJsonPathComponents(const string& path, vector<string>* components);

}
//////////////////////////////////////////////////////////////////////////////////////////
// FindJsonPathComponents

TEST(FindJsonPathComponents, BasicTest) {
  const string path = "hello.world";
  vector<string> components;
  FindJsonPathComponents(path, &components);
  EXPECT_EQ(components.size(), 2);
  EXPECT_EQ(components[0], "hello");
  EXPECT_EQ(components[1], "world");
}

TEST(FindJsonPathComponents, EnclosingQuotes) {
  vector<string> components;
  FindJsonPathComponents("\"hello.world\"", &components);
  EXPECT_EQ(components.size(), 1);
  EXPECT_EQ("hello.world", components[0]);

  components.clear();
  FindJsonPathComponents("\"\"", &components);
  EXPECT_EQ(components.size(), 0);

  components.clear();
  FindJsonPathComponents("\"hello.world", &components);
  EXPECT_EQ(components.size(), 1);
  EXPECT_EQ("\"hello.world", components[0]);
}

//////////////////////////////////////////////////////////////////////////////////////////
// Templating

void TestTemplate(const string& tmpl, const string& json_context,
    const string& expected) {
  Document document;
  // TODO: Check for error
  document.Parse<0>(json_context.c_str());
  ASSERT_TRUE(document.IsObject()) << json_context << " is not valid json";
  stringstream ss;
  ASSERT_TRUE(RenderTemplate(tmpl, "", document, &ss)) << " parse error! "
                                                       << endl << "Template: " << tmpl
                                                       << ", json: " << json_context;
  ASSERT_EQ(expected, ss.str()) << "Template: " << tmpl << ", json: " << json_context;
}

void TestTemplateExpectError(const string& tmpl, const string& json_context) {
  Document document;
  // TODO: Check for error
  document.Parse<0>(json_context.c_str());
  ASSERT_TRUE(document.IsObject()) << json_context << " is not valid json";
  stringstream ss;
  ASSERT_FALSE(RenderTemplate(tmpl, "", document, &ss)) << "No parse error! "
                                                        << endl << "Template: " << tmpl
                                                        << ", json: " << json_context;
}

TEST(RenderTemplate, SubstituteSimpleTypes) {
  TestTemplate("{{ str_key }}", "{ \"str_key\": \"hello world\" }", "hello world");
  TestTemplate("{{ int_key }}", "{ \"int_key\": 10 }", "10");
  TestTemplate("{{ double_key }}", "{ \"double_key\": 10.0123 }", "10.0123");
  TestTemplate("{{ bool_key }}", "{ \"bool_key\": true }", "true");
  TestTemplate("{{ null_key }}", "{ \"null_key\": null }", "");
}

TEST(RenderTemplate, SubstituteComplexTypes) {
  TestTemplate("{{ obj_key }}", "{ \"obj_key\": { \"a\": 1 }}", "");
  TestTemplate("{{ array_key }}", "{ \"array_key\": [1,2,3,4]}", "");
}

TEST(RenderTemplate, SubstituteWithPaths) {
  TestTemplate("{{ a.b.c }}", "{ \"a\": { \"b\": { \"c\": 10 } } }", "10");

  // Non-existant path
  TestTemplate("{{ a.b.c.d }}", "{ \"a\": { \"b\": { \"c\": 10 } } }", "");
}

TEST(RenderTemplate, WithAsIterator) {
  TestTemplate("{{#a}}Hello{{/a}}", "{ \"a\": [1, 2] }", "HelloHello");
  TestTemplate("{{#a}}{{.}}{{/a}}", "{ \"a\": [1, 2] }", "12");
  TestTemplate("{{#a}}{{foo.bar}}{{/a}}", "{ \"a\": [ {\"foo\": { \"bar\": 1 } }, 2] }",
      "1");

  TestTemplate("{{#a}}{{foo.bar}}{{/a}}{{#b}}{{.}}{{/b}}",
      "{ \"a\": [ {\"foo\": { \"bar\": 1 } }], \"b\": [ 2 ] }",
      "12");

  // Recreate bug found producing HTML
  TestTemplate("{{#a}} {{#b}}b{{/b}} a {{/a}}d", "{ \"a\": [ { \"b\": [] } ] }",
      "  a d");

  TestTemplate("{{#a}}1{{/a}} 2", "{ \"a\": [] }", " 2");
  TestTemplate("{{#a}}{{should-skip}}{{/a}} Hello", "{ \"a\": [] }", " Hello");
}

TEST(RenderTemplate, WithAsPredicate) {
  TestTemplate("{{#a}}Hello{{/a}}", "{ \"a\": 1}", "Hello");
  TestTemplate("{{#a}}Hello {{.}}{{/a}}", "{ \"a\": 1}", "Hello 1");
  TestTemplate("{{#a}}Hello {{#b}}World{{/b}}{{/a}}", "{ \"a\": { \"b\": 2} }",
      "Hello World");
  // 'b' gets resolved from the outer context
  TestTemplate("{{#a}}Hello {{#b}}World{{/b}}{{/a}}", "{ \"a\": 1, \"b\": 2 }",
      "Hello World");
  TestTemplate("{{#a}}Hello{{/a}}", "{ \"a\": true }", "Hello");
  TestTemplate("{{#a}}Hello{{/a}}", "{ \"a\": false }", "");
}

TEST(RenderTemplate, WithAsNegatedPredicate) {
  TestTemplate("{{^a}}Hello{{/a}}", "{ \"a\": 1}", "");
  TestTemplate("{{^a}}Hello {{.}}{{/a}}", "{ \"a\": 1}", "");
  // Empty lists are not false, per RapidJson
  TestTemplate("{{^a}}Hello{{/a}}", "{ \"a\": [] }", "");

  TestTemplate("{{^a}}Hello{{/a}}", "{ \"a\": true }", "");
  TestTemplate("{{^a}}Hello{{/a}}", "{ \"a\": false }", "Hello");

  // Test that matching a negated predicate retains the same level context
  TestTemplate("{{^a}}{{b}}{{/a}}", "{ \"b\": 1}", "1");
}

TEST(RenderTemplate, ContextPreservingPredicate) {
  TestTemplate("{{?a}}{{b}}{{/a}}", "{ \"a\": { \"b\": 10}, \"b\": 20 }", "20");
  // Just for comparison: note difference when using # operator
  TestTemplate("{{#a}}{{b}}{{/a}}", "{ \"a\": { \"b\": 10}, \"b\": 20 }", "10");

  TestTemplate("{{?a}}{{b}}{{/a}}", "{ \"a\": { \"b\": 10} }", "");
  TestTemplate("{{?a}}{{b}}{{/a}}", "{ \"c\": { \"b\": 10}, \"b\": 20 }", "");
}

TEST(RenderTemplate, ResolveFromParentContext) {
  TestTemplate("{{#a}}{{b}}{{/a}}", "{ \"a\": { \"b\": 10}, \"b\": 20 }", "10");
  // 'c' should be resolved from parent.
  TestTemplate("{{#a}}{{c}}{{/a}}", "{ \"a\": { }, \"c\": 20 }", "20");
  // 'c.x' should be resolved from parent.
  TestTemplate("{{#a}}{{c.x}}{{/a}}", "{ \"a\": { }, \"c\": { \"x\": 20 } }", "20");
}


TEST(RenderTemplate, IgnoredBlocks) {
  TestTemplate("Hello {{!ignoreme }} world", "{ \"ignoreme\": 1 }", "Hello  world");
}

TEST(RenderTemplate, Escaping) {
  TestTemplate("{{escape}}", "{ \"escape\": \"<html>\" }", "&lt;html&gt;");
  TestTemplate("{{{escape}}}", "{ \"escape\": \"<html>\" }", "<html>");
}

TEST(RenderTemplate, Partials) {
  TestTemplate("{{>doesntexist.tmpl}}", "{ }", "");
  TestTemplate("{{>test-templates/partial.tmpl}}", "{ \"a\": 10 }", "Hello 10");
  TestTemplate("{{a}}{{>test-templates/partial.tmpl}}{{a}}", "{ \"a\": 10 }",
      "10Hello 1010");
  TestTemplate("{{#outer}} {{>test-templates/partial.tmpl}} {{/outer}}",
      "{ \"outer\": { \"a\": 42 } }", " Hello 42 ");
  TestTemplate("{{>test-templates/partial.tmpl}}", "{ \"outer\": { \"a\": 42 } }",
      "Hello ");
  TestTemplate("{{#outer}}{{>test-templates/partial.tmpl}}{{/outer}}",
      "{ \"outer\": [1,2,3] }", "Hello Hello Hello ");

  // Check that if a template is not found, <name>.mustache is also tried.
  TestTemplate("{{>test-templates/mst-template}}", "{ }", "Hello world");
}

TEST(RenderTemplate, Length) {
  TestTemplate("{{%bar}}", "{ \"bar\": [1, 2, 3] }", "3");
  TestTemplate("{{%bar}}", "{ \"bar\": [] }", "0");
  TestTemplate("{{%bar}}", "{ \"bar\": \"abcdefg\" }", "7");
  TestTemplate("{{%bar}}", "{ \"bar\": 8 }", "");
  TestTemplate("{{%bar}}", "{ \"foo\": \"abcdefg\" }", "");
}

TEST(RenderTemplate, Literal) {
  TestTemplate("{{~bar}}", "{ \"bar\": [3] }", "[\n    3\n]");
  TestTemplate("{{~bar}}", "{ \"bar\": 3 }", "");
  TestTemplate("{{~bar}}", "{ \"bar\": { \"foo\": 3 } }", "{\n    \"foo\": 3\n}");
}

TEST(RenderTemplate, Equality) {
  TestTemplate("{{=bar foo}}true{{/bar}}", "{ \"bar\": \"foo\" }", "true");
  TestTemplate("{{=bar foo}}true{{/bar}}", "{ \"bar\": \"baz\" }", "");
  TestTemplate("{{!=bar foo}}true{{/bar}}", "{ \"bar\": \"baz\" }", "true");
  TestTemplate("{{!=bar foo}}true{{/bar}}", "{ \"bar\": \"foo\" }", "");

  // Test case-insensitivity
  TestTemplate("{{=bar FoO}}true{{/bar}}", "{ \"bar\": \"foo\" }", "true");

  TestTemplate("{{=bar foo  }}true{{/bar}}", "{ \"bar\": \"foo\" }", "true");

  TestTemplate("{{=bar foo}}{{=baz bar}}true{{/baz}}{{/bar}}",
      "{ \"bar\": \"foo\", \"baz\": \"bar\" }", "true");

  TestTemplate("{{=bar foo}}", "{ }", "");
}

TEST(RenderTemplate, Int64) {
  int64_t int64 = std::numeric_limits<int64_t>::max();
  stringstream json;
  json << "{ \"bar\": " << int64 << " }";
  stringstream ss;
  ss << int64;
  TestTemplate("{{bar}}", json.str(), ss.str());
}

TEST(RenderTemplate, NestedTemplatesWithSameTag) {
  TestTemplate("{{?b}}{{#b}}{{/b}}{{/b}}Hello", "{ \"b\": 3 }", "Hello");
  TestTemplate("{{?b}}{{#b}}{{/b}}{{/b}}Hello", "{  }", "Hello");
}

TEST(Errors, BasicErrors) {
  TestTemplateExpectError("{{?b}}{{/a}}", "{ }");
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
