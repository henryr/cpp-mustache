#include "gtest/gtest.h"
#include "rapidjson/document.h"

#include <vector>

using namespace rapidjson;
using namespace std;

void FindJsonPathComponents(const string& path, vector<string>* components);
void RenderTemplate(const string& document, const Value& context, stringstream* out);

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
  RenderTemplate(tmpl, document, &ss);
  ASSERT_EQ(expected, ss.str()) << "Template: " << tmpl << ", json: " << json_context;
}

TEST(RenderTemplate, SubstituteSimpleTypes) {
  TestTemplate("{{ str_key }}", "{ \"str_key\": \"hello world\" }", "hello world");
  TestTemplate("{{ int_key }}", "{ \"int_key\": 10 }", "10");
  TestTemplate("{{ double_key }}", "{ \"double_key\": 10.0123 }", "10.0123");
  TestTemplate("{{ bool_key }}", "{ \"bool_key\": true }", "1");
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


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
