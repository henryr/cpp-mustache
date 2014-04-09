#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include <iostream>
#include <sstream>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

using namespace rapidjson;
using namespace std;
using namespace boost::algorithm;

int Dispatch(const string& document, int idx, const Value* context, char tag,
    const string& tag_name, stringstream* out);

bool IsTag(char tag) {
  return tag == '!' || tag == '#' || tag == '^' || tag == '>' || tag == '/';
}

void FindJsonPathComponents(const string& path, vector<string>* components) {
  //
  bool in_quote = false;
  bool in_escape = false;
  int start = 0;
  for (int i = 0; i < path.size(); ++i) {
    if (path[i] == '"' && !in_escape) {
      in_quote = !in_quote;
    }

    if (path[i] == '.' && !in_escape && !in_quote) {
      if (i - start > 0) {
        if (path[start] == '"' && path[(i - 1) - start] == '"') {
          if (i - start > 3) components->push_back(path.substr(start + 1, i - (start + 2)));
        } else {
          components->push_back(path.substr(start, i - start));
        }
        start = i + 1;
      }
    }

    if (path[i] == '\\' && !in_escape) {
      in_escape = true;
    } else {
      in_escape = false;
    }
  }

  if (path.size() - start > 0) {
    components->push_back(path.substr(start, path.size() - start));
  }
}

void ResolveJsonContext(const string& path, const Value& parent_context,
    const Value** resolved) {
  if (path == ".") {
    *resolved = &parent_context;
    return;
  }
  vector<string> components;
  FindJsonPathComponents(path, &components);
  const Value* cur = &parent_context;
  BOOST_FOREACH(const string& c, components) {
    if (cur->IsObject() && cur->HasMember(c.c_str())) {
      cur = &(*cur)[c.c_str()];
    } else {
      *resolved = NULL;
      return;
    }
  }

  *resolved = cur;
}

int FindNextMustache(const string& document, int idx, char* tag, string* tag_name,
    stringstream* out) {
  while (idx < document.size()) {
    if (document[idx] == '{' && idx < (document.size() - 3) && document[idx + 1] == '{') {
      idx += 2; // Now at start of template expression
      stringstream expr;
      while (idx < document.size()) {
        if (document[idx] != '}') {
          expr << document[idx];
          ++idx;
        } else {
          if (idx < document.size() - 1 && document[idx + 1] == '}') {
            ++idx;
            break;
          } else {
            expr << '}';
          }
        }
      }

      string key = expr.str();
      trim(key);
      if (key.size() == 0) continue;
      char tag_candidate = key[0];
      if (IsTag(tag_candidate)) {
        *tag = tag_candidate;
        key = key.substr(1);
        trim(key);
      } else {
        *tag = 0;
      }
      if (key.size() == 0) continue;
      *tag_name = key;
      return ++idx;
    } else {
      if (out != NULL) (*out) << document[idx];
    }
    ++idx;
  }
  return idx;
}

int DoWith(const string& document, int idx, const Value* parent_context,
    const string& tag_name, stringstream* out) {
  // Precondition: idx is the immedate next character after an opening {{ #tag_name }}
  // Context is the
  const Value* context;
  ResolveJsonContext(tag_name, *parent_context, &context);

  bool blank = (context == NULL);
  vector<const Value*> values;
  if (context != NULL && context->IsArray()) {
    for (int i = 0; i < context->Size(); ++i) {
      values.push_back(&(*context)[i]);
    }
  } else {
    values.push_back(context);
  }
  int start_idx = idx;
  BOOST_FOREACH(const Value* v, values) {
    idx = start_idx;
    while (idx < document.size()) {
      char tag = 0;
      string next_tag_name;
      idx = FindNextMustache(document, idx, &tag, &next_tag_name, blank ? NULL : out);

      if (idx > document.size()) {
        cout << "Gone off end of document" << endl;
        return idx;
      }
      // Nested with templates won't be seen here, they'll all be processed inside
      // Dispatch(). So any end-with template must correspond to this with.
      if (tag == '/') {
        if (next_tag_name != tag_name) return -1;
        break;
      }
      idx = Dispatch(document, idx, v, tag, next_tag_name, blank ? NULL : out);
    }
  }
  return idx;
}

int DoSubstitute(const string& document, const int idx, const Value* parent_context,
    const string& tag_name, stringstream* out) {
  const Value* context;
  ResolveJsonContext(tag_name, *parent_context, &context);
  if (context == NULL) return idx;
  if (context->IsString()) {
    (*out) << context->GetString();
  } else if (context->IsNumber()) {
    (*out) << context->GetInt();
  } // TODO
  return idx;
}

int Dispatch(const string& document, int idx, const Value* context, char tag, const string& tag_name, stringstream* out) {
  switch (tag) {
    case '#':
      return DoWith(document, idx, context, tag_name, out);
    case 0:
      return DoSubstitute(document, idx, context, tag_name, out);
    case '!':
      return idx; // Ignored
    case '>':
      return idx; // TODO: Partials
    default:
      cout << "Unknown tag: " << tag << endl;
      return -1;
  }
}

void RenderTemplate(const string& document, const Value& context, stringstream* out) {
  int idx = 0;
  while (idx < document.size() && idx != -1) {
    string tag_name;
    char tag;
    idx = FindNextMustache(document, idx, &tag, &tag_name, out);
    cout << "Found: " << tag_name << ", tag: " << tag << endl;
    idx = Dispatch(document, idx, &context, tag, tag_name, out);
  }
}

const string TMPL = "abcdefg {{ contacts }} {{ ! contacts }} hijk {{contacts}} "
"{{#lst}}"
"hey everyone {{.}} {{unknown}}"
"{{/lst}}"

"{{ foo.bar}}";

int main(int argc, char** argv) {
  rapidjson::Document d;
  d.SetObject().AddMember("contacts", 10, d.GetAllocator());
  Value obj(kObjectType);
  obj.AddMember("bar", 11, d.GetAllocator());
  d.AddMember("foo", obj, d.GetAllocator());

  Value lst(kArrayType);
  lst.PushBack(0, d.GetAllocator());
  lst.PushBack(1, d.GetAllocator());
  d.AddMember("lst", lst, d.GetAllocator());

  StringBuffer buffer;
  Writer<StringBuffer> writer(buffer);
  d.Accept(writer);
  cout << buffer.GetString() << endl;

  stringstream ss;
  RenderTemplate(TMPL, d, &ss);
  cout << ss.str() << endl;
  // vector<string> components;
  // FindJsonPathComponents("\"hello.world\".how.are.you", &components);
  // BOOST_FOREACH(const string& c, components) {
  //   cout << c << endl;
  // }
}
