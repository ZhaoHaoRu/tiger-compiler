#include "tiger/frame/temp.h"

#include <cstdio>
#include <set>
#include <sstream>

namespace temp {

LabelFactory LabelFactory::label_factory;
TempFactory TempFactory::temp_factory;

Label *LabelFactory::NewLabel() {
  char buf[100];
  sprintf(buf, "L%d", label_factory.label_id_++);
  return NamedLabel(std::string(buf));
}

/**
 * Get symbol of a label_. The label_ will be created only if it is not found.
 * @param s label_ string
 * @return symbol
 */
Label *LabelFactory::NamedLabel(std::string_view s) {
  return sym::Symbol::UniqueSymbol(s);
}

std::string LabelFactory::LabelString(Label *s) { return s->Name(); }

Temp *TempFactory::NewTemp() {
  Temp *p = new Temp(temp_factory.temp_id_++);
  std::stringstream stream;
  stream << 't';
  stream << p->num_;
  Map::Name()->Enter(p, new std::string(stream.str()));

  return p;
}

int Temp::Int() const { return num_; }

Map *Map::Empty() { return new Map(); }

Map *Map::Name() {
  static Map *m = nullptr;
  if (!m)
    m = Empty();
  return m;
}

Map *Map::LayerMap(Map *over, Map *under) {
  if (over == nullptr)
    return under;
  else
    return new Map(over->tab_, LayerMap(over->under_, under));
}

void Map::Enter(Temp *t, std::string *s) {
  assert(tab_);
  tab_->Enter(t, s);
}

std::string *Map::Look(Temp *t) {
  std::string *s;
  assert(tab_);
  s = tab_->Look(t);
  if (s)
    return s;
  else if (under_)
    return under_->Look(t);
  else
    return nullptr;
}

void Map::DumpMap(FILE *out) {
  tab_->Dump([out](temp::Temp *t, std::string *r) {
    fprintf(out, "t%d -> %s\n", t->Int(), r->data());
  });
  if (under_) {
    fprintf(out, "---------\n");
    under_->DumpMap(out);
  }
}

bool TempList::Contain(Temp *new_temp) {
  assert(new_temp != nullptr);
  for(auto temp : temp_list_) {
    if(temp->Int() == new_temp->Int()) {
      return true;
    }
  }
  return false;
}

TempList *TempList::Union(TempList *new_temp_list) {
  auto res = new TempList();
  for(auto temp : temp_list_) {
    res->Append(temp);
  }
  if(new_temp_list) {
    auto temps = new_temp_list->GetList();
    for(auto temp : temps) {
      if (!temp) {
        printf("in union nullptr!\n");
      }
      if(!res->Contain(temp)) {
        res->Append(temp);
      }
    }
  }
  
  return res;
}

TempList *TempList::Diff(TempList *new_temp_list) {
  auto res = new TempList();
  for(auto temp : temp_list_) {
    if (!temp) {
      printf("in diff nullptr\n");
      continue;
    }
    if(!new_temp_list->Contain(temp)) {
      res->Append(temp);
    }
  }
  return res;
}

bool TempList::CheckEmpty() {
  for(auto temp : temp_list_) {
    if (!temp) {
      return true;
    }
  }
  return false;
}

bool TempList::Equal(TempList *new_temp_list) {
  auto another_temp_list = new_temp_list->GetList();

  if(temp_list_.size() != another_temp_list.size()) {
    return false;
  }

  auto it1 = temp_list_.begin();
  auto it2 = another_temp_list.begin();

  for(auto elem : temp_list_) {
    if (!elem) {
        printf("in equal nullptr!\n");
    }
    if(!new_temp_list->Contain(elem)) {
      return false;
    }
  }
  return true;
}

TempList *TempList::ReplaceTemp(temp::Temp *old_temp, temp::Temp *new_temp) {
  temp::TempList *new_templist = new temp::TempList();
  for(auto &temp : temp_list_) {
    if(temp == old_temp) {
      new_templist->Append(new_temp);
    } else {
      new_templist->Append(temp);
    }
  }
  return new_templist;
}

} // namespace temp