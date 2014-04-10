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
// # Handle malformed templates better
// # Support array_tag.length?

enum TagOperator {
  SUBSTITUTION,
  SECTION_START,
  NEGATED_SECTION_START,
  SECTION_END,
  PARTIAL,
  COMMENT
};

TagOperator GetOperator(const string& tag) {
  if (tag.size() == 0) return SUBSTITUTION;
  switch (tag[0]) {
    case '#': return SECTION_START;
    case '^': return NEGATED_SECTION_START;
    case '/': return SECTION_END;
    case '>': return PARTIAL;
    case '!': return COMMENT;
    default: return SUBSTITUTION;
  }
}

int EvaluateTag(const string& document, int idx, const Value* context, TagOperator tag,
    const string& tag_name, stringstream* out);

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

int FindNextTag(const string& document, int idx, TagOperator* tag_op, string* tag_name,
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
      *tag_op = GetOperator(key);
      if (*tag_op != SUBSTITUTION) {
        key = key.substr(1);
        trim(key);
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

int EvaluateSection(const string& document, int idx, const Value* parent_context,
    bool is_negation, const string& tag_name, stringstream* out) {
  // Precondition: idx is the immedate next character after an opening {{ #tag_name }}
  const Value* context;
  ResolveJsonContext(tag_name, *parent_context, &context);

  // If we a) cannot resolve the context from the tag name or b) the context evaluates to
  // false, we should skip the contents of the template until a closing {{/tag_name}}.
  bool skip_contents = (context == NULL || context->IsFalse());

  // If the tag is a negative block (i.e. {{^tag_name}}), do the opposite: if the context
  // exists and is true, skip the contents, else echo them.
  if (is_negation) skip_contents = !skip_contents;

  vector<const Value*> values;
  if (!skip_contents && context != NULL && context->IsArray()) {
    for (int i = 0; i < context->Size(); ++i) {
      values.push_back(&(*context)[i]);
    }
  } else {
    values.push_back(skip_contents ? NULL : context);
  }
  int start_idx = idx;
  BOOST_FOREACH(const Value* v, values) {
    idx = start_idx;
    while (idx < document.size()) {
      TagOperator tag_op;
      string next_tag_name;
      idx = FindNextTag(document, idx, &tag_op, &next_tag_name, NULL,
          skip_contents ? NULL : out);

      if (idx > document.size()) return idx;
      if (tag_op == SECTION_END && next_tag_name == tag_name) {
        break;
      }

      // Don't need to evaluate any templates if we're skipping the contents
      if (!skip_contents) {
        idx = EvaluateTag(document, idx, v, tag_op, next_tag_name, out);
      }
    }
  }
  return idx;
}

int EvaluateSubstitution(const string& document, const int idx,
    const Value* parent_context, const string& tag_name, stringstream* out) {
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

int EvaluateTag(const string& document, int idx, const Value* context, TagOperator tag,
    const string& tag_name, stringstream* out) {
  if (idx == -1) return idx;
  switch (tag) {
    case SECTION_START:
      return EvaluateSection(document, idx, context, false, tag_name, out);
    case NEGATED_SECTION_START:
      return EvaluateSection(document, idx, context, true, tag_name, out);
    case SUBSTITUTION:
      return EvaluateSubstitution(document, idx, context, tag_name, out);
    case COMMENT:
      return idx; // Ignored
    case PARTIAL:
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
    TagOperator tag_op;
    idx = FindNextTag(document, idx, &tag_op, &tag_name, NULL, out);
    idx = EvaluateTag(document, idx, &context, tag_op, tag_name, out);
  }
}

}
