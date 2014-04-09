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

int FindNextMustache(const string& document, int idx, char* tag, string* tag_name,
    stringstream* out) {
  cout << "Parsing string: " << document << endl;
  while (idx < document.size()) {
    if (document[idx] == '{' && idx < (document.size() - 3) && document[idx + 1] == '{') {
      cout << "Found mustache: " << idx << endl;;
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
      cout << "Returning: " << *tag_name << " - tag: " << *tag << endl;
      return ++idx;
    } else {
      (*out) << document[idx];
    }
    ++idx;
  }
  return idx;
}

int DoWith(const string& document, int idx, const Value* parent_context,
    const string& tag_name, stringstream* out) {
  // Precondition: idx is the immedate next character after an opening {{ #tag_name }}
  // block
  const Value* context = NULL;
  if (parent_context->HasMember(tag_name.c_str())) {
    context = &(*parent_context)[tag_name.c_str()];
  }
  vector<const Value*> values;
  if (context->IsArray()) {
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
      idx = FindNextMustache(document, idx, &tag, &next_tag_name, out);

      if (idx >= document.size()) return idx;

      if (tag == '/') {
        if (next_tag_name != tag_name) return -1;
        return idx;
      }
      idx = Dispatch(document, idx, v, tag, tag_name, out);
    }
  }
}

int DoSubstitute(const string& document, const int idx, const Value* context,
    const string& tag_name, stringstream* out) {
  if (!context->HasMember(tag_name.c_str())) {
    return idx;
  }
  const Value& value = (*context)[tag_name.c_str()];
  if (value.IsString()) {
    (*out) << value.GetString();
  } else if (value.IsNumber()) {
    (*out) << value.GetInt();
  } // TODO
  cout << "Returning idx: " << idx << endl;
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
    idx = Dispatch(document, idx, &context, tag, tag_name, out);
    cout << "IDX: " << idx << " SIZE: " << document.size() << endl;
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
