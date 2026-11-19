#ifndef __HASH_TABLE_H__
#define __HASH_TABLE_H__

#include "mixins.h"
#include "GeneralContainer.h"
#include "AVLtree.h"
#include "../utilities/persistence.h"

// Cada bucket de la tabla es un AVL_Tree; sus traits reutilizan el
// comparador del HashTable para mantener ordenados los nodos colisionados.
/*template <typename Traits>
struct HashTableBucketTraits : public BinaryTreeTraits<typename Traits::value_type> {
    using value_type = typename Traits::value_type;
    using Compare    = typename Traits::Compare;
};

template <typename Traits, TX Dir>
class HashTableIterator;

template <typename Traits>
using HashTableForwardIterator = HashTableIterator<Traits, 0>;

template <typename Traits>
using HashTableBackwardIterator = HashTableIterator<Traits, 1>;

// Traits del HashTable: el Node es el mismo BinaryTreeNode que usa el AVL
// y el iterador recorre los buckets en orden (y dentro de cada bucket en
// in-order). La función de dispersión es configurable (std::hash por defecto).
template <typename T, typename _Compare = std::less<T>, typename _Hash = std::hash<T>>
struct HashTableTraits : public DefaultTraits<T, _Compare> {
    using value_type        = T;
    using Compare           = _Compare;
    using Hash              = _Hash;
    using Node              = BinaryTreeNode<T>;
    using BucketTraits      = HashTableBucketTraits<HashTableTraits>;
    using Bucket            = AVL_Tree<BucketTraits>;
    using ForwardIterator   = HashTableForwardIterator<HashTableTraits>;
    using BackwardIterator  = HashTableBackwardIterator<HashTableTraits>;
};

template <typename T, typename _Hash = std::hash<T>>
using HashTableAscTraits = HashTableTraits<T, std::less<T>, _Hash>;

template <typename T, typename _Hash = std::hash<T>>
using HashTableDescTraits = HashTableTraits<T, std::greater<T>, _Hash>;*/

template <typename Traits, template <typename> typename... Mixins>
class BaseHashTable;

template <typename Traits>
using HashTable = BaseHashTable<Traits>;

// Tabla hash con encadenamiento separado: cada bucket es un AVL_Tree.
// Al insertar se actualiza la referencia si la clave ya existe, por lo que
// la tabla se comporta como un mapa clave -> ref sin claves duplicadas.
template <typename Traits, template <typename> typename... Mixins>
class BaseHashTable : public GeneralContainer<Traits, Mixins...> {
public:
    using MySelf = BaseHashTable;
    using Parent = GeneralContainer<Traits, Mixins...>;
    using typename Parent::value_type;
    using typename Parent::Node;
    using typename Parent::NodePtr;
    using ForwardIterator   = typename Traits::ForwardIterator;
    using BackwardIterator = typename Traits::BackwardIterator;
    using Compare = typename Traits::Compare;
    using Order = typename Traits::Order;
private:
    AVL_Tree<Traits> m_avl;
public:
    void push_back(const value_type& value, Ref ref) override;
    Ref& operator[](value_type key);
    friend std::ostream& operator<<(std::ostream& os, MySelf& hash_table) { return os << hash_table.m_avl; }
    friend std::istream& operator>>(std::istream& is, MySelf& hash_table) { return is >> hash_table.m_avl; }
    virtual void clear() {};
    virtual ForwardIterator begin() const {};
    virtual ForwardIterator end() const {};
};

template <typename Traits, template <typename> typename... Mixins>
void BaseHashTable<Traits, Mixins...>::push_back(const value_type& value, Ref ref) {
    m_avl.insert(value, ref);
}

template <typename Traits, template <typename> typename... Mixins>
Ref& BaseHashTable<Traits, Mixins...>::operator[](value_type key) {
    auto nodeprt = m_avl.findNodePtr(key);
    if (nodeprt) return nodeprt->ref();
    throw std::runtime_error("ningun elemento satisface la llave");
}

#endif // __HASH_TABLE_H__
