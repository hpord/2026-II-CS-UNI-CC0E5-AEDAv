#ifndef __MERKLE_TREE_H__
#define __MERKLE_TREE_H__

#include "binarytree.h"
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

// Traits del MerkleTree: reutiliza los nodos, iteradores y recorridos del
// BinaryTree, y agrega la funcion de dispersion con la que se calculan los
// hashes.
template <typename T, typename _Hash = std::hash<T>>
struct MerkleTreeTraits : public BinaryTreeTraits<T> {
    using Hash = _Hash;
};

template <typename T, typename _Hash = std::hash<T>>
struct MerkleTreeAscTraits : public AscendingTraits<T>, public MerkleTreeTraits<T, _Hash> {};

template <typename T, typename _Hash = std::hash<T>>
struct MerkleTreeDescTraits : public DescendingTraits<T>, public MerkleTreeTraits<T, _Hash> {};

// Un paso de la prueba de Merkle: el hash del hermano en ese nivel y el
// valor/referencia del padre, necesario porque cada nodo tambien aporta
// su propio valor al hash.
template <typename T>
struct MerkleProofStep {
    T           parentValue;
    Ref         parentRef;
    std::size_t siblingHash;
    bool        siblingIsRight;
};

// Prueba de inclusion de un nodo. Incluye los hashes de sus hijos para el
// caso en que el nodo probado no sea una hoja, y el camino hacia la raiz.
template <typename T>
struct MerkleProof {
    T           value;
    Ref         ref;
    std::size_t leftHash;
    std::size_t rightHash;
    std::vector<MerkleProofStep<T>> path;
};

template <typename Traits>
class MerkleTree : public BinaryTree<Traits> {
public:
    using MySelf = MerkleTree;
    using Parent = BinaryTree<Traits>;
    using typename Parent::value_type;
    using typename Parent::Node;
    using typename Parent::NodePtr;
    using ForwardIterator   = typename Traits::ForwardIterator;
    using BackwardIterator  = typename Traits::BackwardIterator;
    using Compare           = typename Traits::Compare;
    using Order             = typename Traits::Order;
    using Hash              = typename Traits::Hash;
    using Proof             = MerkleProof<value_type>;

protected:
    using Parent::m_mutex;
    using Parent::m_root;

    static std::size_t combine(std::size_t left, std::size_t right) {
        return left ^ (right + 0x9e3779b97f4a7c15ULL + (left << 6) + (left >> 2));
    }

    static std::size_t valueHash(const value_type& value, Ref ref) {
        return combine(Hash{}(value), std::hash<Ref>{}(ref));
    }

    // El hash de un nodo cubre su propio valor y los hashes de sus hijos.
    static std::size_t nodeHash(const value_type& value, Ref ref,
                                std::size_t leftHash, std::size_t rightHash) {
        return combine(valueHash(value, ref), combine(leftHash, rightHash));
    }

    static NodePtr cloneTree(NodePtr nodeptr, NodePtr parent) {
        if (!nodeptr) return nullptr;
        auto copy = new Node(nodeptr->getValue(), nodeptr->getRef());
        copy->m_parent = parent;
        copy->m_height = nodeptr->m_height;
        copy->m_hash   = nodeptr->m_hash;
        copy->m_pChild[0] = cloneTree(nodeptr->m_pChild[0], copy);
        copy->m_pChild[1] = cloneTree(nodeptr->m_pChild[1], copy);
        return copy;
    }

    void updateHash(NodePtr nodeptr);
    void refreshHashes(NodePtr nodeptr);
    bool verifyNode(NodePtr nodeptr) const;
    NodePtr findNodePtrConst(const value_type& value) const;

public:
    MerkleTree() = default;
    MerkleTree(const MerkleTree& another) : ContainerCore<Traits>(), Parent() {
        std::scoped_lock lock(this->m_mutex, another.m_mutex);
        m_root = cloneTree(another.m_root, nullptr);
    }
    MerkleTree(MerkleTree&& another) noexcept : Parent(std::move(another)) {}
    virtual ~MerkleTree() = default;

    // La copia preserva la estructura y, por tanto, el hash raiz.
    MerkleTree& operator=(const MerkleTree& another) {
        if (this == &another) return *this;
        std::scoped_lock lock(this->m_mutex, another.m_mutex);
        Parent::clear();
        m_root = cloneTree(another.m_root, nullptr);
        return *this;
    }

    MerkleTree& operator=(MerkleTree&& another) noexcept {
        Parent::operator=(std::move(another));
        return *this;
    }

    // Se ejecuta al finalizar cada insercion (de abajo hacia arriba), por lo
    // que los hashes de los hijos ya estan actualizados.
    void Redistribution(NodePtr nodeptr) override { updateHash(nodeptr); }

    // Recalcula los hashes despues de modificar los valores en sitio.
    template <typename Func, typename... Args>
    void ApplyFunction(Func func, Args&&... args) {
        std::lock_guard lock(this->m_mutex);
        Parent::ApplyFunction(std::forward<Func>(func), std::forward<Args>(args)...);
        refreshHashes(m_root);
    }

    std::size_t rootHash() const;
    bool verify() const;

    Proof getProof(const value_type& value) const;
    bool verifyProof(const Proof& proof, std::size_t expectedRootHash) const;
    bool verifyProof(const Proof& proof) const { return verifyProof(proof, rootHash()); }
};

template <typename Traits>
void MerkleTree<Traits>::updateHash(NodePtr nodeptr) {
    if (!nodeptr) return;
    auto leftHash  = nodeptr->m_pChild[0] ? nodeptr->m_pChild[0]->m_hash : 0;
    auto rightHash = nodeptr->m_pChild[1] ? nodeptr->m_pChild[1]->m_hash : 0;
    nodeptr->m_hash = nodeHash(nodeptr->getValue(), nodeptr->getRef(), leftHash, rightHash);
}

template <typename Traits>
void MerkleTree<Traits>::refreshHashes(NodePtr nodeptr) {
    if (!nodeptr) return;
    refreshHashes(nodeptr->m_pChild[0]);
    refreshHashes(nodeptr->m_pChild[1]);
    updateHash(nodeptr);
}

template <typename Traits>
bool MerkleTree<Traits>::verifyNode(NodePtr nodeptr) const {
    if (!nodeptr) return true;
    if (!verifyNode(nodeptr->m_pChild[0]) || !verifyNode(nodeptr->m_pChild[1])) return false;

    auto leftHash  = nodeptr->m_pChild[0] ? nodeptr->m_pChild[0]->m_hash : 0;
    auto rightHash = nodeptr->m_pChild[1] ? nodeptr->m_pChild[1]->m_hash : 0;
    return nodeptr->m_hash == nodeHash(nodeptr->getValue(), nodeptr->getRef(), leftHash, rightHash);
}

template <typename Traits>
auto MerkleTree<Traits>::findNodePtrConst(const value_type& value) const -> NodePtr {
    auto current = m_root;
    while (current) {
        auto branch = Compare()(current->getValue(), value);
        if (!branch && !Compare()(value, current->getValue())) return current;
        current = current->m_pChild[branch];
    }
    return current;
}

template <typename Traits>
std::size_t MerkleTree<Traits>::rootHash() const {
    std::lock_guard lock(this->m_mutex);
    return m_root ? m_root->m_hash : 0;
}

template <typename Traits>
bool MerkleTree<Traits>::verify() const {
    std::lock_guard lock(this->m_mutex);
    return verifyNode(m_root);
}

template <typename Traits>
auto MerkleTree<Traits>::getProof(const value_type& value) const -> Proof {
    std::lock_guard lock(this->m_mutex);
    auto nodeptr = findNodePtrConst(value);
    if (!nodeptr) throw std::runtime_error("MerkleTree::getProof(): valor no encontrado");

    auto leftHash  = nodeptr->m_pChild[0] ? nodeptr->m_pChild[0]->m_hash : 0;
    auto rightHash = nodeptr->m_pChild[1] ? nodeptr->m_pChild[1]->m_hash : 0;
    Proof proof{nodeptr->getValue(), nodeptr->getRef(), leftHash, rightHash, {}};

    while (nodeptr->m_parent) {
        auto parent = nodeptr->m_parent;
        auto nodeIsRight = nodeptr == parent->m_pChild[1];
        auto sibling = parent->m_pChild[1 - nodeIsRight];
        proof.path.push_back({parent->getValue(), parent->getRef(),
                              sibling ? sibling->m_hash : 0, !nodeIsRight});
        nodeptr = parent;
    }
    return proof;
}

template <typename Traits>
bool MerkleTree<Traits>::verifyProof(const Proof& proof, std::size_t expectedRootHash) const {
    auto hash = nodeHash(proof.value, proof.ref, proof.leftHash, proof.rightHash);
    for (const auto& step : proof.path) {
        auto leftHash  = step.siblingIsRight ? hash : step.siblingHash;
        auto rightHash = step.siblingIsRight ? step.siblingHash : hash;
        hash = nodeHash(step.parentValue, step.parentRef, leftHash, rightHash);
    }
    return hash == expectedRootHash;
}

#endif // __MERKLE_TREE_H__
