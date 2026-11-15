#include <iostream>
#include <fstream> // ofstream para escribir en archivo
#include <string>
#include <thread>
#include <vector>
#include <map>
#include "foreach.h"
#include "containers/vector.h"
#include "containers/linkedlist.h"
#include "containers/doublelinkedlist.h"
#include "containers/circularlinkedlist.h"
#include "containers/circulardoublelinkedlist.h"
#include "containers/Heap.h"
#include "containers/binarytree.h"
#include "containers/AVLtree.h"
#include "containers/hashtable.h"
#include "utilities/printcolored.h"
#include "Demos.h"
using namespace std;

void AddOne(GeneralNode<TX> &node) {
    node.value() += 1;
}

template <typename T>
void AddX(GeneralNode<T> &node, T x) {
    node.value() += x;
}

void Square(GeneralNode<TX> &node) {
    node.value() *= node.value();
}

template <typename T>
void ModX(GeneralNode<T> &node, T x) {
    node.value() %= x;
}

// Predicado generico para FirstThat/call: true si el valor del nodo es
// mayor que 'threshold'. Generico en Node y T para servir tanto a
// Vector<VectorAscTraits<TX>> (int) como a Vector<VectorAscTraits<string>>.
template <typename Node, typename T>
bool IsGreaterThan(Node &node, T threshold) {
    return node.getValue() > threshold;
}

template <typename Node>
void PrintNode(Node &node, ostream &os) {
    os << node << " ";
}
const TX NThreads = 5;

// Inserta secuencialmente cada pareja (valor, ref) de 'values' en el
// Container, una por una. La insercion concurrente queda aislada en
// DemoRaceCondition(), que es donde se estudia esa problematica.
template <typename Container>
void InsertElements(Container &container,
                     const vector<pair<typename Container::value_type, Ref>> &values) {
    for (const auto &v : values)
        container.push_back(v.first, v.second);
}

template <typename Container>
void TestContainer(Container &container,
                    const string &filename) {
    //InsertElements(container, values);

    // Impresion usando operador de inserción
    cout << "Container using operator<<: ";
    cout << container;
    cout << endl;

    // Escritura hacia un archivo, en modo append para acumular cada estado
    // (el archivo se deja vacio una vez al inicio, ver DemoVector)
    ofstream of(filename, ios::app);
    of << container;
    of << endl;
    of.close();

    // Escritura en pantalla usando cout directamente (operator<<)
    cout << "Container using cout directly: ";
    cout << container << endl;
}

// Prueba los recorridos del Container hacia adelante (begin/end) y hacia
// atras (rbegin/rend) solamente, imprimiendo los elementos en cada sentido.
template <typename Container>
void TestTraversal(Container &container) {
    using Node = typename Container::Node;

    cout << "Forward traversal:  [";
    container.call(PrintNode<Node>, cout);
    cout << "]" << endl;

    cout << "Backward traversal: [";
    container.rcall(PrintNode<Node>, cout);
    cout << "]" << endl;

    // Prueba de call()/FirstThat() con valor de retorno: busca el primer
    // nodo cuyo valor sea mayor que el del primer elemento del container.
    if (!container.empty()) {
        auto threshold = (*container.begin()).getValue();
        Node &found = container.FirstThat(IsGreaterThan<Node, decltype(threshold)>, threshold);
        cout << "FirstThat (primer valor > " << threshold << "): " << found << endl;
    }
}

// Prueba los recorridos hacia únicamente hacia adelante, especialmente para una LinkedList
template <typename Container>
void TestForwardTraversal(Container &container){
    using Node = typename Container::Node;

    cout << "Prueba de función call con std::cout para LinkedList [";
    container.call(PrintNode<Node>, cout);
    cout << "]" << endl;

    cout << "Prueba de uso de iteradores para impresión: [";
    for (auto &&node : container)
        cout << node << " ";
    cout << "]" << endl;

    // Prueba de FirstThat(): primer nodo cuyo valor sea mayor que el primero
    if (container.begin() != container.end()) {
        auto threshold = (*container.begin()).getValue();
        Node &found = container.FirstThat(IsGreaterThan<Node, decltype(threshold)>, threshold);
        cout << "FirstThat (primer valor > " << threshold << "): " << found << endl;
    }
}

void DemoVector() {
    PrintColored(cout, "Pruebas del contenedor Vector", Color::Yellow);
    // Dejamos los archivos vacios para que TestContainer acumule (append)
    // el estado del container tras cada paso
    ofstream("vector.txt", ios::trunc).close();

    // Cada elemento es una pareja (valor, ref) que se guarda en un Node;
    // el constructor initializer_list arma el Vector inicial de una vez
    Vector<VectorAscTraits<TX>> vec({{0, 10}, {1, 11}, {2, 12}, {3, 13}, {4, 14}});
    InsertElements(vec, {{5, 15}, {6, 16}, {7, 17}, {8, 18}, {9, 19}});
    TestContainer(vec, "vector.txt");
    TestTraversal(vec);

    ofstream("vector_str.txt", ios::trunc).close();
    Vector<VectorAscTraits<string>> strVec;
    InsertElements(strVec, {{"Hello", 1}, {"World", 2}});
    TestContainer(strVec, "vector_str.txt");
    TestTraversal(strVec);
}

void TestCustomVector() {
    BaseVector<VectorAscTraits<TX>, Callable, Applicable, Searchable, ReverseCallable> vec({{0, 10}, {1, 11}, {2, 12}, {3, 13}, {4, 14}});

}

void DemoCustomContainer() {
    TestCustomVector();
}

// Insertamos muchos elementos (generados en un loop, no a mano)
// concurrentemente y comparamos cuantos deberian haber entrado contra
// cuantos entraron realmente. Si push_back no estuviera sincronizado
// (sin el mutex/lock_guard actual) esto perderia inserciones o crashearia;
// con el mutex protegiendo push_back/resize, deberia dar siempre 0 perdidas.
void DemoRaceCondition() {
    const size_t N = 200000;

    vector<pair<TX, Ref>> values(N);
    for (size_t i = 0; i < N; ++i)
        values[i] = {static_cast<TX>(i), static_cast<Ref>(i)};

    Vector<VectorAscTraits<TX>> vec;

    // Insercion concurrente: NThreads workers insertando en paralelo sobre
    // el mismo Vector, cada uno con un subconjunto entrelazado (stride)
    size_t n = values.size();
    vector<thread> workers;
    for (size_t t = 0; t < NThreads; ++t) {
        workers.emplace_back([&vec, &values, n, t](){
            for (size_t i = t; i < n; i += NThreads)
                vec.push_back(values[i].first, values[i].second);
        });
    }
    for (auto &worker : workers)
        worker.join();

    TXX expectedSum = 0;
    for (auto &v : values) expectedSum += v.first;

    TXX actualSum = 0;
    for (size_t i = 0; i < vec.size(); ++i) actualSum += vec[i].getValue();

    cout << "DemoRaceCondition: se esperaban " << N << " elementos, "
         << "el Vector quedo con " << vec.size() << endl;
    cout << "  suma esperada = " << expectedSum
         << ", suma obtenida = " << actualSum << endl;

    if (vec.size() != N || actualSum != expectedSum)
        cout << "  *** Race condition detectada: se perdieron inserciones (push_back / resize sin sincronizar) ***" << endl;
    else
        cout << "  No se perdio ningun elemento: el mutex de push_back/resize "
             << "sincroniza correctamente las inserciones concurrentes" << endl;
}

template <typename Container>
void TestLinkedList(Container &list, const string &filename) {
    ofstream(filename, ios::trunc).close();

    // Pruebas de push_back
    InsertElements(list, {{5, 15}, {1, 11}, {8, 18}, {3, 13}});
    TestContainer(list, filename);
    TestForwardTraversal(list);

    InsertElements(list, {{0, 10}, {1, 11}, {2, 12}, {3, 13}, {4, 14}});

    // Pruebas de insert
    list.insert(4, 12);
    list.insert(11, 20);
    list.insert(7, 6);
    cout << "Lista luego de hacer 3 inserts: " << list << endl;

    // Prueba de ApplyFunction
    list.ApplyFunction(AddOne);
    cout << "Lista tras usar AddOne a sus elementos: " << list << endl;
    list.ApplyFunction(AddX<TX>, TX(10));
    cout << "Lista tras usar AddX(10) a sus elementos: " << list << endl;

    // Prueba de clear
    list.clear();
    cout << "Lista luego de usar clear: " << list << endl;
    list.push_back(10, 2);
    cout << "Lista luego de un nuevo push_back: " << list << endl;

    // Prueba de lectura y asignacion (operator=)
    Container new_list;
    ifstream in(filename);
    in >> new_list;
    in.close();
    cout << "Lista leida desde archivo: " << new_list << endl;
    TestForwardTraversal(new_list);

    list = new_list;
}

template <typename DoubleContainer>
void TestDoubleLinkedListSpecifics(DoubleContainer &list) {
    TestTraversal(list);

    list.clear();
    list.insert(5, 50);
    list.insert(1, 10);
    list.insert(9, 90);
    list.insert(6, 60);
    cout << "Lista tras inserts en cabeza, cola y medio: " << list << endl;
    TestTraversal(list);
}

void DemoLinkedList()
{
    cout << "\n";
    PrintColored(cout, "Pruebas para LinkedList", Color::Yellow);
    LinkedList<LinkedListAscTraits<TX>> list;
    TestLinkedList(list, "linkedlist.txt");

    cout << "\n";
    PrintColored(cout, "Pruebas para DoubleLinkedList", Color::Yellow);
    DoubleLinkedList<DoubleLinkedListAscTraits<TX>> dlist;
    TestLinkedList(dlist, "doublelinkedlist.txt");
    TestDoubleLinkedListSpecifics(dlist);

    cout << "\n";
    PrintColored(cout, "Pruebas para CircularLinkedList", Color::Yellow);
    CircularLinkedList<LinkedListAscTraits<TX>> clist;
    TestLinkedList(clist, "circularlinkedlist.txt");

    cout << "\n";
    PrintColored(cout, "Pruebas para CircularDoubleLinkedList", Color::Yellow);
    CircularDoubleLinkedList<DoubleLinkedListAscTraits<TX>> cdlist;
    TestLinkedList(cdlist, "circulardoublelinkedlist.txt");
    TestDoubleLinkedListSpecifics(cdlist);
}

template <typename Heap>
void InitHeap(Heap &heap) {
    heap.push(50, 5);
    heap.push(20, 2);
    heap.push(80, 8);
    heap.push(10, 1);
    heap.push(30, 3);
    heap.push(70, 7);
    cout << "Tras inserciones: " << heap << endl;
}

template <typename Heap>
void ExtractHeap(Heap &heap) {
    cout << "Tras extracción: ";
    while (!heap.empty()) {
        auto node = heap.pop();
        cout << node.getValue() << " ";
    }
    cout << endl;
}

template <typename Heap>
void TestHeap(Heap& heap) {
    InitHeap(heap);
    ExtractHeap(heap);
    InitHeap(heap);
    heap.ApplyFunction(AddOne);
    cout << "Tras aplicar AddOne: " << heap << endl;
    TX m = 13;
    heap.ApplyFunction(ModX<TX>, TX(m));
    cout << "Tras aplicar mod " << m << ": " << heap << endl;
    ExtractHeap(heap);
    cout << endl;
}

void DemoHeap() {
    cout << "\n";
    PrintColored(cout, "Pruebas para MinHeap", Color::Yellow);
    MinHeap minHeap;
    TestHeap(minHeap);

    cout << "\n";
    PrintColored(cout, "Pruebas para MaxHeap", Color::Yellow);
    MaxHeap maxHeap;
    TestHeap(maxHeap);
}

template <typename TreeContainer>
void TestBinaryTree(TreeContainer &tree, const string &filename) {
    ofstream(filename, ios::trunc).close();

    // Pruebas de insercion y persistencia
    //InsertElements(tree, {{50, 5}, {20, 2}, {80, 8}, {10, 1}, {30, 3}, {70, 7}, {90, 9}});
    InsertElements(tree, {{40, 6}, {20, 2}, {10, 1}, {25, 4}, {30, 5}, {22, 3}, {50, 7}});
    //InsertElements(tree, {{40, 6}, {20, 2}, {10, 1}, {50, 4}});
    //InsertElements(tree, {{10, 6}, {20, 2}, {40, 1}});
    TestContainer(tree, filename);
    // Recorrido in-order
    TestTraversal(tree);

    print_tree(tree);
    cout << endl;

    cout  << "Aumentar 11\n";
    tree.ApplyFunction(AddOne);
    cout << "Arbol tras usar AddOne: " << tree << endl;
    tree.ApplyFunction(AddX<TX>, TX(10));
    cout << "Arbol tras usar AddX(10): " << tree << endl;

    print_tree(tree);
    cout << endl;

    cout << "IN ORDER\n";
    for (auto& elem: tree.InOrder()) {
        cout << elem << " ";
    }
    cout << endl;
    cout << "IN ORDER Reverse\n";
    for (auto& elem: tree.ReverseInOrder()) {
        cout << elem << " ";
    }
     cout << endl;

     cout << "PRE ORDER\n";
     for (auto& elem: tree.PreOrder()) {
         cout << elem << " ";
     }
     cout << endl;
     cout << "PRE ORDER Reverse\n";
     for (auto& elem: tree.ReversePreOrder()) {
         cout << elem << " ";
     }
      cout << endl;

      cout << "POST ORDER\n";
      for (auto& elem: tree.PostOrder()) {
          cout << elem << " ";
      }
      cout << endl;
      cout << "IN ORDER Reverse\n";
      for (auto& elem: tree.ReversePostOrder()) {
          cout << elem << " ";
      }
       cout << endl;
    /*tree.ApplyFunction(AddOne);
    cout << "Arbol tras usar AddOne: " << tree << endl;
    tree.ApplyFunction(AddX<TX>, TX(10));
    cout << "Arbol tras usar AddX(10): " << tree << endl;*/
    cout << endl;

    tree.remove(41);
    cout << "Remover 41\n\n";
    print_tree(tree);
    cout << endl;

    tree.remove(31);
    cout << "Remover 31\n\n";
    print_tree(tree);
    cout << endl;

    tree.remove(51);
    cout << "Remover 51\n\n";
    print_tree(tree);
    cout << endl;

    tree.remove(61);
    cout << "Remover 61\n\n";
    print_tree(tree);
    cout << endl;

    tree.clear();
    cout << "Arbol luego de usar clear: " << tree << endl;

    // lectura
    TreeContainer new_tree;
    ifstream in(filename);
    in >> new_tree;
    in.close();
    cout << "Arbol leido desde archivo: " << new_tree << endl;

    tree = new_tree;
    cout << "Arbol tras operator=: " << tree << endl;

    cout << "\n\n";
    print_tree(tree);
    cout << endl;

}

void DemoBinaryTree() {
    cout << "\n";
    PrintColored(cout, "Pruebas para BinaryTree", Color::Yellow);
    BinaryTree<BinaryTreeAscTraits<TX>> tree;
    TestBinaryTree(tree, "binarytree.txt");
}

void DemoAVLTree() {
    cout << "\n";
    PrintColored(cout, "Pruebas para AVLTree", Color::Yellow);
    AVL_Tree<BinaryTreeAscTraits<TX>> avltree;
    TestBinaryTree(avltree, "binarytree.txt");
}

void DemoHashTable() {
    cout << "\n";
    PrintColored(cout, "Pruebas para HashTable (buckets AVL)", Color::Yellow);
    ofstream("hashtable.txt", ios::trunc).close();

    // Insercion de elementos; la tabla crece sola al superar el load factor.
    HashTable<HashTableAscTraits<TX>> table;
    InsertElements(table, {{40, 6}, {15, 2}, {70, 7}, {25, 4}, {55, 5}, {22, 3}, {50, 8}, {10, 1}, {30, 9}});

    // Capacidad pequena a proposito para que se vean las colisiones:
    // cada bucket es un AVL y guarda sus nodos ordenados.
    table.rehash(7);

    TestContainer(table, "hashtable.txt");

    // Recorridos: como el orden depende de los buckets (no es globalmente
    // ordenado como en un arbol), no usamos TestTraversal, cuyo FirstThat
    // asume que el primer elemento es el minimo.
    cout << "Forward traversal:  [";
    table.call(PrintNode<typename HashTable<HashTableAscTraits<TX>>::Node>, cout);
    cout << "]" << endl;
    cout << "Backward traversal: [";
    table.rcall(PrintNode<typename HashTable<HashTableAscTraits<TX>>::Node>, cout);
    cout << "]" << endl;
    auto& firstEven = table.FirstThat([](auto& node) { return node.getValue() % 2 == 0; });
    cout << "FirstThat (primer valor par): " << firstEven << endl;

    cout << "Factor de carga: " << table.load_factor() << endl;

    print_hash_table(table);

    // Busqueda directa por clave (usa el hash, no recorre toda la tabla)
    cout << "Buscar 70: " << table.search(70) << endl;
    cout << "Remover 40: " << table.remove(40) << endl;
    cout << "Buscar 40: " << (table.findNodePtr(40) ? "encontrado" : "no encontrado") << endl;

    // push_back con una clave existente actualiza su referencia
    table.push_back(15, 99);
    cout << "Tras push_back(15,99): " << table << endl;

    // ApplyFunction reconstruye los buckets porque cambian las claves
    table.ApplyFunction(AddOne);
    cout << "Tras aplicar AddOne: " << table << endl;

    // Lectura y asignacion
    HashTable<HashTableAscTraits<TX>> new_table;
    ifstream in("hashtable.txt");
    in >> new_table;
    in.close();
    cout << "Tabla leida desde archivo: " << new_table << endl;
    table = new_table;
    cout << "Tabla tras operator=: " << table << endl;

    print_hash_table(table);
}
int suma(int a, int b)           {    return a+b;    }
int resta(int a, int b)          {    return a-b;    }
int multiplicacion(int a, int b) {    return a*b;    }
int division(int a, int b)       {    return a/b;    }

void DemoPointersToFunctions(){
    cout << "\n";
    PrintColored(cout, "Pruebas para Punteros a Funciones", Color::Yellow);
    int (*pFunc1)(int,int) = &suma;
    int x = 5, y = 10;
    cout << "Suma(1) de " << x << " y " << y << " es: " << pFunc1(x,y) << endl;

    auto pFunc2 = &suma;
    cout << "Suma de " << x << " y " << y << " es: " << pFunc2(x,y) << endl;

    int ope;
    cout << "Ingrese operacion a realizar (0: suma, 1: resta, 2: multiplicacion, 3: division): ";
    cin >> ope;
    switch(ope){
        case 0: pFunc1 = &suma;           break;
        case 1: pFunc1 = &resta;          break;
        case 2: pFunc1 = &multiplicacion; break;
        case 3: pFunc1 = &division;       break;
        default: cout << "Operacion invalida" << endl; return;
    }
    cout << "Suma (switch) de " << x << " y " << y << " es: " << pFunc1(x,y) << endl;

    int (*vpf[4])(int,int) = {&suma, &resta, &multiplicacion, &division};
    cout << "Ingrese operacion a realizar (0: suma, 1: resta, 2: multiplicacion, 3: division): ";
    cin >> ope;
    int rpta = (*vpf[ope])(x,y);
    cout << "Suma (vpf): " << rpta << endl;

    using FuncPtr = int(*)(int,int);
    map<int, FuncPtr> m1 = {
        {0, &suma},
        {5, &resta},
        {8, &multiplicacion},
        {9, &division}
    };
    cout << "Ingrese operacion a realizar (0: suma, 5: resta, 8: multiplicacion, 9: division): ";
    cin >> ope;
    rpta = m[ope](x,y);
    cout << "Suma (map): " << rpta << endl;

    enum OPE_TYPE {
        SUMA = 0,
        RESTA = 1,
        MULTIPLICACION = 2,
        DIVISION = 3
    };
    map<OPE_TYPE, FuncPtr> m2 = {
        {OPE_TYPE::SUMA, &suma},
        {OPE_TYPE::RESTA, &resta},
        {OPE_TYPE::MULTIPLICACION, &multiplicacion},
        {OPE_TYPE::DIVISION, &division}
    };
    cout << "Ingrese operacion a realizar (" << OPE_TYPE::SUMA << ": suma, " << OPE_TYPE::RESTA << ": resta, " << OPE_TYPE::MULTIPLICACION << ": multiplicacion, " << OPE_TYPE::DIVISION << ": division): ";
    cin >> ope;
    rpta = m2[ope](x,y);
    cout << "Suma (map con enum): " << rpta << endl;
}
