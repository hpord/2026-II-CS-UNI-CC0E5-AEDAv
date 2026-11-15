# Diagramas de clases

Diagramas de clases en **Mermaid** para el código C++ del proyecto, con foco en el **BinaryTree**, el contenedor **AVL_Tree**, la **HashTable** que usa AVL como buckets y sus clases padres, así como para los **iteradores** y los **traits** (tipos de datos que describen la estrategia de recorrido y comparación de cada contenedor).

## 1. BinaryTree, AVL_Tree y sus padres

```mermaid
classDiagram
    class ContainerCore {
        <<abstract>>
        +T value_type
        +Node Node
        +NodePtr NodePtr
        +Delim Delim
        +ForwardIterator ForwardIterator
        +empty() bool
        +clear()
        +push_back(T value, Ref ref)
        +begin() ForwardIterator
        +end() ForwardIterator
    }
    class GeneralContainer {
        +T value_type
        +Node Node
        +NodePtr NodePtr
        +Delim Delim
        +ForwardIterator ForwardIterator
    }
    class Callable {
        +call(Func func, Args... args)
    }
    class Applicable {
        +ApplyFunction(Func func, Args... args)
    }
    class Searchable {
        +FirstThat(Func func, Args... args) Node
    }
    class ReverseCallable {
        +rcall(Func func, Args... args)
    }
    class Printable {
    }
    class Readable {
    }
    class BaseBinaryTree {
        +insert(T value, Ref ref)
        +clear()
        +push_back(T value, Ref ref)
        +begin() ForwardIterator
        +end() ForwardIterator
        +rbegin() BackwardIterator
        +rend() BackwardIterator
        +InOrder() BinaryTreeProxy
        +ReverseInOrder() BinaryTreeProxy
        +PreOrder() BinaryTreeProxy
        +ReversePreOrder() BinaryTreeProxy
        +PostOrder() BinaryTreeProxy
        +ReversePostOrder() BinaryTreeProxy
        +findNodePtr(T value) NodePtr
        +remove(T value) Node
        +height(NodePtr nodeptr) TX
        +Redistribution(NodePtr nodeptr) void
    }
    class BinaryTree {
    }
    class AVL_Tree {
        +rotate(NodePtr nodeptr, TX dir)
        +balance(NodePtr nodeptr) TX
        +Redistribution(NodePtr nodeptr) void
    }
    ContainerCore <|-- GeneralContainer : herencia del core de contenedores
    Callable <|-- ContainerCore
    Applicable <|-- ContainerCore
    Searchable <|-- ContainerCore
    ReverseCallable <|-- ContainerCore
    Printable <|-- ContainerCore
    Readable <|-- ContainerCore
    Callable <|-- GeneralContainer
    Applicable <|-- GeneralContainer
    Searchable <|-- GeneralContainer
    ReverseCallable <|-- GeneralContainer
    Printable <|-- GeneralContainer
    Readable <|-- GeneralContainer
    GeneralContainer <|-- BaseBinaryTree : herencia del arbol
    BaseBinaryTree <|-- BinaryTree : alias con mixins por defecto
    BinaryTree <|-- AVL_Tree : AVL sobre BT
```

## 2. Nodo del BinaryTree

```mermaid
classDiagram
    class GeneralNode {
        -T m_value
        -Ref m_ref
        +getValue() T
        +getRef() Ref
    }
    class BinaryTreeNode {
        +BinaryTreeNode m_pChild
        +BinaryTreeNode m_parent
        +TX m_height
    }
    GeneralNode <|-- BinaryTreeNode
```

## 3. Iteradores del BinaryTree

```mermaid
classDiagram
    class TransversarOrder {
        <<enumeration>>
        PreOrder
        InOrder
        PostOrder
    }
    class GeneralIterator {
        <<abstract>>
        -T m_ptr
        -T m_pStart
        +operator*() T
    }
    class BinaryTreeIterator {
        -TransversarOrder m_TrOrder
        +operator++() BinaryTreeIterator
        +setTrOrder(TransversarOrder order)
    }
    class BinaryTreeForwardIterator {
    }
    class BinaryTreeBackwardIterator {
    }
    class BinaryTreeProxy {
        +BinaryTreeProxy(Iterator begin, Iterator end)
        +begin() Iterator
        +end() Iterator
    }
    GeneralIterator <|-- BinaryTreeIterator
    BinaryTreeIterator <|-- BinaryTreeForwardIterator
    BinaryTreeIterator <|-- BinaryTreeBackwardIterator
```

## 4. Traits del BinaryTree

```mermaid
classDiagram
    class DefaultTraits {
        +T value_type
        +Compare Compare
    }
    class AscendingTraits {
    }
    class DescendingTraits {
    }
    class BinaryTreeTraits {
        +Node Node
        +ForwardIterator ForwardIterator
        +BackwardIterator BackwardIterator
        +Order Order
    }
    class BinaryTreeAscTraits {
    }
    class BinaryTreeDescTraits {
    }
    DefaultTraits <|-- AscendingTraits
    DefaultTraits <|-- DescendingTraits
    AscendingTraits <|-- BinaryTreeAscTraits
    BinaryTreeTraits <|-- BinaryTreeAscTraits
    AscendingTraits <|-- BinaryTreeDescTraits
    BinaryTreeTraits <|-- BinaryTreeDescTraits
```

## 5. HashTable con buckets AVL

`BaseHashTable` resuelve las colisiones con encadenamiento separado: la tabla
es un arreglo dinámico de `AVL_Tree`, uno por bucket. La clave se dispersa con
`Hash{}(value) % m_capacity`; cuando el factor de carga supera `0.75` la tabla
hace rehash a la siguiente capacidad prima.

```mermaid
classDiagram
    class HashTableTraits {
        +T value_type
        +Compare Compare
        +Hash Hash
        +Node Node
        +Bucket Bucket
        +ForwardIterator ForwardIterator
        +BackwardIterator BackwardIterator
    }
    class HashTableBucketTraits {
        +T value_type
        +Compare Compare
    }
    class BaseHashTable {
        -Bucket* m_buckets
        -size_t m_capacity
        -size_t m_size
        +push_back(T value, Ref ref)
        +insert(T value, Ref ref) bool
        +search(T value) Node
        +findNodePtr(T value) NodePtr
        +remove(T value) Node
        +rehash(size_t newCapacity)
        +size() size_t
        +capacity() size_t
        +load_factor() double
    }
    class HashTableIterator {
        -size_t m_bucket
        -size_t m_bucketCount
        +operator++() HashTableIterator
    }
    class AVL_Tree {
        +rotate(NodePtr nodeptr, TX dir)
        +balance(NodePtr nodeptr) TX
    }
    DefaultTraits <|-- HashTableTraits
    BinaryTreeTraits <|-- HashTableBucketTraits
    GeneralContainer <|-- BaseHashTable
    BaseHashTable *-- AVL_Tree : buckets
    HashTableBucketTraits --> AVL_Tree : describe el bucket
    GeneralIterator <|-- HashTableIterator
    BaseHashTable --> HashTableIterator : begin/end
```

## Fuentes

- `containers/binarytree.h`
- `containers/AVLtree.h`
- `containers/hashtable.h`
- `containers/GeneralContainer.h`
- `containers/ContainerCore.h`
- `containers/mixins.h`
- `containers/GeneralIterator.h`
- `containers/GeneralNode.h`
