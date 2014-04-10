// Copyright 2014 Henry Robinson
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "mustache.h"

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include <iostream>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

using namespace rapidjson;
using namespace std;
using namespace boost::algorithm;

namespace mustache {

// TODO:
// # Triple {{{
// # ^ operator
// # Ignore false #

int Dispatch(const string& document, int idx, const Value* context, char tag,
    const string& tag_name, stringstream* out);

bool IsTag(char tag) {
  return tag == '!' || tag == '#' || tag == '^' || tag == '>' || tag == '/';
}

void EscapeHtml(const string& in, stringstream *out) {
  BOOST_FOREACH(const char& c, in) {
    switch (c) {
      case '&': (*out) << "&amp;";
        break;
      case '"': (*out) << "&quot;";
        break;
      case '\'': (*out) << "&apos;";
        break;
      case '<': (*out) << "&lt;";
        break;
      case '>': (*out) << "&gt;";
        break;
      default: (*out) << c;
        break;
    }
  }
}

void FindJsonPathComponents(const string& path, vector<string>* components) {
  bool in_quote = false;
  bool escape_this_char = false;
  int start = 0;
  for (int i = start; i < path.size(); ++i) {
    if (path[i] == '"' && !escape_this_char) in_quote = !in_quote;
    if (path[i] == '.' && !escape_this_char && !in_quote) {
      // Current char == delimiter and not escaped and not Found the end of a path
      // component =>
      if (i - start > 0) {
        if (path[start] == '"' && path[(i - 1) - start] == '"') {
          if (i - start > 3) {
            components->push_back(path.substr(start + 1, i - (start + 2)));
          }
        } else {
          components->push_back(path.substr(start, i - start));
        }
        start = i + 1;
      }
    }

    escape_this_char = (path[i] == '\\' && !escape_this_char);
  }

  if (path.size() - start > 0) {
    if (path[start] == '"' && path[(path.size() - 1) - start] == '"') {
      if (path.size() - start > 3) {
        components->push_back(path.substr(start + 1, path.size() - (start + 2)));
      }
    } else {
      components->push_back(path.substr(start, path.size() - start));
    }
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
    bool* is_triple, stringstream* out) {
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
      if (key != ".") trim_if(key, is_any_of("."));
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
      idx = FindNextMustache(document, idx, &tag, &next_tag_name, NULL,
          blank ? NULL : out);

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
    if (true) {
      EscapeHtml(context->GetString(), out);
    } else {
      // TODO: Triple {{{ means don't escape
      (*out) << context->GetString();
    }
  } else if (context->IsInt()) {
    (*out) << context->GetInt();
  } else if (context->IsDouble()) {
    (*out) << context->GetDouble();
  } else if (context->IsBool()) {
    (*out) << context->GetBool();
  }
  return idx;
}

int Dispatch(const string& document, int idx, const Value* context, char tag,
    const string& tag_name, stringstream* out) {
  if (idx == -1) return idx;
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
    idx = FindNextMustache(document, idx, &tag, &tag_name, NULL, out);
    idx = Dispatch(document, idx, &context, tag, tag_name, out);
  }
}

}
