#ifndef __HASH_TABLE_H__
#define __HASH_TABLE_H__
#include "mixins.h"
#include "GeneralContainer.h"
#include "AVLtree.h"

template <typename Traits, template <typename> typename... Mixins>
class BaseHashTable;

template <typename Traits>
using HashTable = BaseHashTable<Traits, Callable, Searchable>;

template <typename Traits, template <typename> typename... Mixins>
class BaseHashTable : public GeneralContainer<Traits, Mixins...>  {
    ;
};

#endif // __HASH_TABLE_H__
