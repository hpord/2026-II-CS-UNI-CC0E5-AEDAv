#ifndef __HASH_TABLE_H__
#define __HASH_TABLE_H__
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include "mixins.h"
#include "GeneralContainer.h"
#include "AVLtree.h"

// Cada bucket de la tabla es un AVL_Tree; sus traits reutilizan el
// comparador del HashTable para mantener ordenados los nodos colisionados.
template <typename Traits>
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
using HashTableDescTraits = HashTableTraits<T, std::greater<T>, _Hash>;

// Iterador que recorre la tabla bucket por bucket. Dentro de cada bucket
// delega en el iterador in-order del AVL (normal o inverso según Dir).
template <typename Traits, TX Dir>
class HashTableIterator : public GeneralIterator<HashTableIterator<Traits, Dir>, typename Traits::Node> {
public:
    using Node              = typename Traits::Node;
    using Bucket            = typename Traits::Bucket;
    using MySelf            = HashTableIterator<Traits, Dir>;
    using Parent            = GeneralIterator<MySelf, Node>;
    using BucketIterator    = std::conditional_t<Dir == 0,
                                                 typename Bucket::ForwardIterator,
                                                 typename Bucket::BackwardIterator>;
private:
    const Bucket* m_pBuckets = nullptr;
    size_t        m_bucketCount = 0;
    size_t        m_bucket = 0;
    BucketIterator m_iter;

    // Mantiene sincronizado el puntero que usa GeneralIterator para
    // comparar iteradores (m_ptr == nullptr representa el final).
    void sync() {
        Parent::m_ptr = (m_pBuckets && m_iter != BucketIterator(nullptr)) ? &(*m_iter) : nullptr;
    }

    // Avanza al siguiente bucket no vacío en la dirección de recorrido.
    bool seekNextBucket() {
        while (true) {
            if constexpr (Dir == 0) {
                if (m_bucket + 1 >= m_bucketCount) return false;
                ++m_bucket;
                m_iter = m_pBuckets[m_bucket].begin();
            } else {
                if (m_bucket == 0) return false;
                --m_bucket;
                m_iter = m_pBuckets[m_bucket].rbegin();
            }
            if (m_iter != BucketIterator(nullptr)) return true;
        }
    }

public:
    HashTableIterator() : Parent(nullptr), m_iter(nullptr) {}

    HashTableIterator(const Bucket* buckets, size_t count)
        : Parent(nullptr), m_pBuckets(buckets), m_bucketCount(count), m_iter(nullptr) {
        if (!m_pBuckets || m_bucketCount == 0) return;

        if constexpr (Dir == 0) {
            for (m_bucket = 0; m_bucket < m_bucketCount; ++m_bucket) {
                m_iter = m_pBuckets[m_bucket].begin();
                if (m_iter != BucketIterator(nullptr)) { sync(); return; }
            }
        } else {
            m_bucket = m_bucketCount;
            while (m_bucket > 0) {
                --m_bucket;
                m_iter = m_pBuckets[m_bucket].rbegin();
                if (m_iter != BucketIterator(nullptr)) { sync(); return; }
            }
        }
    }

    MySelf& operator++() {
        if (!Parent::m_ptr) return *this;

        ++m_iter;
        if (m_iter != BucketIterator(nullptr)) {
            sync();
            return *this;
        }

        if (!seekNextBucket()) {
            Parent::m_ptr = nullptr;
            m_iter = BucketIterator(nullptr);
            return *this;
        }

        sync();
        return *this;
    }
};

template <typename Traits, template <typename> typename... Mixins>
class BaseHashTable;

template <typename Traits>
using HashTable = BaseHashTable<Traits, Callable, Applicable, Searchable, ReverseCallable, Printable, Readable>;

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
    using BackwardIterator  = typename Traits::BackwardIterator;
    using Compare           = typename Traits::Compare;
    using Hash              = typename Traits::Hash;
    using Bucket            = typename Traits::Bucket;

protected:
    using Parent::m_mutex;
    static constexpr size_t INITIAL_CAPACITY = 11;   // primo inicial
    static constexpr double MAX_LOAD_FACTOR  = 0.75; // umbral de rehash

    Bucket* m_buckets  = nullptr;
    size_t  m_capacity = 0;
    size_t  m_size     = 0;

    size_t indexOf(const value_type& value, size_t capacity) const {
        return Hash{}(value) % capacity;
    }

    static size_t nextPrime(size_t n);
    void resizeBuckets(size_t newCapacity); // asume m_mutex tomado
    bool internalInsert(const value_type& value, Ref ref);

public:
    BaseHashTable() = default;
    BaseHashTable(const BaseHashTable& another) { *this = another; }
    BaseHashTable(BaseHashTable&& another) noexcept { *this = std::move(another); }
    BaseHashTable(std::initializer_list<std::pair<value_type, Ref>> values) {
        for (const auto& v : values)
            push_back(v.first, v.second);
    }
    virtual ~BaseHashTable() { clear(); }

    BaseHashTable& operator=(const BaseHashTable& another);
    BaseHashTable& operator=(BaseHashTable&& another) noexcept;

    // Inserción (push_back es la interfaz común de todos los contenedores).
    void push_back(const value_type& value, Ref ref) override { internalInsert(value, ref); }
    bool insert(const value_type& value, Ref ref) { return internalInsert(value, ref); }

    void clear() override;
    size_t size() const {
        std::lock_guard lock(this->m_mutex);
        return m_size;
    }
    size_t capacity() const {
        std::lock_guard lock(this->m_mutex);
        return m_capacity;
    }
    double load_factor() const {
        std::lock_guard lock(this->m_mutex);
        return m_capacity ? static_cast<double>(m_size) / m_capacity : 0.0;
    }

    // Reconstruye la tabla con otra capacidad (rehash).
    void rehash(size_t newCapacity) {
        std::lock_guard lock(this->m_mutex);
        resizeBuckets(newCapacity);
    }

    NodePtr findNodePtr(const value_type& value);
    bool contains(const value_type& value) { return findNodePtr(value) != nullptr; }
    Node& search(const value_type& value);
    Node remove(const value_type& value);

    ForwardIterator begin() const override { return ForwardIterator(m_buckets, m_capacity); }
    ForwardIterator end()   const override { return ForwardIterator(); }
    BackwardIterator rbegin() const { return BackwardIterator(m_buckets, m_capacity); }
    BackwardIterator rend()   const { return BackwardIterator(); }

    // Si la función modifica los valores, las claves cambian de hash: hay
    // que reconstruir los buckets para restaurar la invariante de la tabla.
    template <typename Func, typename... Args>
    void ApplyFunction(Func func, Args&&... args) {
        std::lock_guard lock(this->m_mutex);
        Parent::ApplyFunction(func, std::forward<Args>(args)...);
        if (m_capacity) resizeBuckets(m_capacity);
    }

    template <typename Traits2, template <typename> typename... Mixins2>
    friend void print_hash_table(const BaseHashTable<Traits2, Mixins2...>& table);
};

template <typename Traits, template <typename> typename... Mixins>
auto BaseHashTable<Traits, Mixins...>::operator=(const BaseHashTable& another) -> BaseHashTable& {
    this->copy_from(another);
    return *this;
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseHashTable<Traits, Mixins...>::operator=(BaseHashTable&& another) noexcept -> BaseHashTable& {
    if (this == &another) return *this;
    std::scoped_lock lock(this->m_mutex, another.m_mutex);
    clear();

    m_buckets  = std::exchange(another.m_buckets, nullptr);
    m_capacity = std::exchange(another.m_capacity, 0);
    m_size     = std::exchange(another.m_size, 0);
    return *this;
}

template <typename Traits, template <typename> typename... Mixins>
size_t BaseHashTable<Traits, Mixins...>::nextPrime(size_t n) {
    auto isPrime = [](size_t x) {
        if (x < 2) return false;
        for (size_t d = 2; d * d <= x; ++d)
            if (x % d == 0) return false;
        return true;
    };
    while (!isPrime(n)) ++n;
    return n;
}

template <typename Traits, template <typename> typename... Mixins>
void BaseHashTable<Traits, Mixins...>::clear() {
    std::lock_guard lock(this->m_mutex);
    delete[] m_buckets;
    m_buckets  = nullptr;
    m_capacity = 0;
    m_size     = 0;
}

template <typename Traits, template <typename> typename... Mixins>
void BaseHashTable<Traits, Mixins...>::resizeBuckets(size_t newCapacity) {
    if (newCapacity == 0) newCapacity = 1;

    Bucket* newBuckets = new Bucket[newCapacity];
    for (size_t i = 0; i < m_capacity; ++i)
        for (const auto& node : m_buckets[i])
            newBuckets[indexOf(node.getValue(), newCapacity)].push_back(node.getValue(), node.getRef());

    delete[] m_buckets;
    m_buckets  = newBuckets;
    m_capacity = newCapacity;
}

template <typename Traits, template <typename> typename... Mixins>
bool BaseHashTable<Traits, Mixins...>::internalInsert(const value_type& value, Ref ref) {
    std::lock_guard lock(this->m_mutex);
    if (m_capacity == 0) resizeBuckets(INITIAL_CAPACITY);

    auto index = indexOf(value, m_capacity);
    if (auto found = m_buckets[index].findNodePtr(value)) {
        found->ref() = ref;
        return false;
    }

    m_buckets[index].push_back(value, ref);
    ++m_size;

    if (static_cast<double>(m_size) / m_capacity > MAX_LOAD_FACTOR)
        resizeBuckets(nextPrime(m_capacity * 2));
    return true;
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseHashTable<Traits, Mixins...>::findNodePtr(const value_type& value) -> NodePtr {
    std::lock_guard lock(this->m_mutex);
    if (m_capacity == 0) return nullptr;
    return m_buckets[indexOf(value, m_capacity)].findNodePtr(value);
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseHashTable<Traits, Mixins...>::search(const value_type& value) -> Node& {
    auto nodeptr = findNodePtr(value);
    if (!nodeptr) throw std::out_of_range("HashTable::search: clave no encontrada");
    return *nodeptr;
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseHashTable<Traits, Mixins...>::remove(const value_type& value) -> Node {
    std::lock_guard lock(this->m_mutex);
    static Node nullNode;
    if (m_capacity == 0) return nullNode;

    auto index = indexOf(value, m_capacity);
    if (!m_buckets[index].findNodePtr(value)) return nullNode;

    Node removed = m_buckets[index].remove(value);
    --m_size;
    return removed;
}

// Imprime la tabla completa: cada bucket no vacío con su AVL dibujado.
template <typename Traits, template <typename> typename... Mixins>
void print_hash_table(const BaseHashTable<Traits, Mixins...>& table) {
    std::lock_guard lock(table.m_mutex);
    std::cout << "HashTable (size = " << table.m_size
              << ", capacity = " << table.m_capacity;
    if (table.m_capacity)
        std::cout << ", load factor = "
                  << static_cast<double>(table.m_size) / table.m_capacity;
    std::cout << ")\n";

    if (!table.m_capacity) {
        std::cout << "La tabla está vacía.\n";
        return;
    }

    for (size_t i = 0; i < table.m_capacity; ++i) {
        size_t count = 0;
        for (const auto& node : table.m_buckets[i]) { (void)node; ++count; }
        if (count == 0) continue;

        std::cout << "Bucket " << i << " (" << count << " nodo(s)): [";
        std::string sep;
        for (const auto& node : table.m_buckets[i]) {
            std::cout << sep << node;
            sep = ", ";
        }
        std::cout << "]\n";
        print_tree(table.m_buckets[i]);
    }
}

#endif // __HASH_TABLE_H__
