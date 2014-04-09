#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include <iostream>
#include <sstream>

#include <boost/algorithm/string.hpp>

using namespace rapidjson;
using namespace std;
using namespace boost::algorithm;

int DoWith(const string& document, int idx, const string& tag_name, const Value& parent_context, stringstream* out) {
  // Precondition: idx is the immedate next character after an opening {{ #tag_name }} block
  Value* context = NULL;
  if (parent_context.HasValue(tag_name.c_str())) {
    context = parent_context[tag_name.c_str()];
  }
  if (context.IsList())
  while (idx < document.size()) {
    char tag;
    string next_tag_name;
    idx = FindNextMustache(document, idx, &tag, &next_tag_name, out);

    if (idx >= document.size()) return idx;

    if (tag == '/') {
      if (next_tag_name != tag_name) return -1;
      return idx;
    }
    Dispatch(document, idx,
  }
}

void RenderTemplate(const string& document, const Value& context, stringstream* out) {
  cout << "Parsing string: " << document << endl;
  unsigned int block_start = 0;
  unsigned int block_end = 0;
  while (block_end < document.size()) {
    cout << "BLOCK_END: " << block_end << endl;
    if (document[block_end] == '{' && block_end < (document.size() - 3) && document[block_end + 1] == '{') {
      cout << "Found mustache: " << block_end;
      block_end += 2; // Now at start of template expression
      stringstream expr;
      while (block_end < document.size()) {
        if (document[block_end] != '}') {
          expr << document[block_end];
          ++block_end;
        } else {
          if (block_end < document.size() - 1 && document[block_end + 1] == '}') {
            ++block_end;
            break;
          } else {
            expr << '}';
          }
        }
      }

      string key = expr.str();
      trim(key);
      if (key.size() == 0) continue;
      char tag = key[0];
      if (tag == '!') continue;
      if (tag == '#' || tag == '^' || tag == '>' || tag == '/') {
        key = key.substr(1);
        trim(key);
      }
      if (key.size() == 0) continue;
      if (context.HasMember(key.c_str())) {
        const Value& value = context[key.c_str()];
        if (value.IsString()) (*out) << value.GetString();
        if (value.IsNumber()) (*out) << value.GetInt();
        // TODO: Other types?
      }
    } else {
      (*out) << document[block_end];
    }
    ++block_end;
  }
}

const string TMPL = "abcdefg {{ contacts }} {{ ! contacts }} hijk {{contacts}} {{ foo}}";

int main(int argc, char** argv) {
  rapidjson::Document d;
  d.SetObject().AddMember("contacts", 10, d.GetAllocator());

  StringBuffer buffer;
  Writer<StringBuffer> writer(buffer);
  d.Accept(writer);
  cout << buffer.GetString() << endl;

  stringstream ss;
  RenderTemplate(TMPL, d, &ss);
  cout << ss.str() << endl;
}
