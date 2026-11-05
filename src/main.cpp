// RefIMP v0.1: a small AST-based interpreter (formative draft).
// The parser and static type checker are deliberately not implemented yet.
// Build: g++ -std=c++17 -Wall -Wextra -pedantic -O2 src/main.cpp -o refimp-ast
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

struct Ref { std::size_t address; };
using Value = std::variant<std::int64_t, bool, Ref>;

std::int64_t integer(const Value& v) {
    if (auto p = std::get_if<std::int64_t>(&v)) return *p;
    throw std::runtime_error("expected an Int value");
}
bool boolean(const Value& v) {
    if (auto p = std::get_if<bool>(&v)) return *p;
    throw std::runtime_error("expected a Bool value");
}
Ref reference(const Value& v) {
    if (auto p = std::get_if<Ref>(&v)) return *p;
    throw std::runtime_error("expected a Ref Int value");
}

struct State {
    // Innermost frame is scopes.back(). Values are mutable but declarations
    // may only shadow bindings in outer frames, never within one frame.
    std::vector<std::unordered_map<std::string, Value>> scopes{1};
    std::unordered_map<std::size_t, std::int64_t> heap;
    std::size_t next_address = 0;

    Value lookup(const std::string& name) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return found->second;
        }
        throw std::runtime_error("unbound variable: " + name);
    }
    void declare(const std::string& name, Value value) {
        auto& frame = scopes.back();
        if (frame.count(name)) throw std::runtime_error("duplicate declaration: " + name);
        frame.emplace(name, std::move(value));
    }
    void assign(const std::string& name, Value value) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) { found->second = std::move(value); return; }
        }
        throw std::runtime_error("assignment to undeclared variable: " + name);
    }
    Ref allocate(std::int64_t n) {
        auto address = next_address++;
        heap[address] = n;
        return Ref{address};
    }
    std::int64_t load(Ref r) const {
        auto it = heap.find(r.address);
        if (it == heap.end()) throw std::runtime_error("invalid heap address");
        return it->second;
    }
    void store(Ref r, std::int64_t n) {
        auto it = heap.find(r.address);
        if (it == heap.end()) throw std::runtime_error("invalid heap address");
        it->second = n;
    }
};

struct Expr {
    virtual ~Expr() = default;
    virtual Value eval(State& state) const = 0;
};
using E = std::shared_ptr<const Expr>;

struct IntLit : Expr {
    std::int64_t value;
    explicit IntLit(std::int64_t n) : value(n) {}
    Value eval(State&) const override { return value; }
};
struct BoolLit : Expr {
    bool value;
    explicit BoolLit(bool b) : value(b) {}
    Value eval(State&) const override { return value; }
};
struct Variable : Expr {
    std::string name;
    explicit Variable(std::string n) : name(std::move(n)) {}
    Value eval(State& s) const override { return s.lookup(name); }
};
enum class Op { Add, Sub, Mul, Le, Eq };
struct Binary : Expr {
    Op op; E left, right;
    Binary(Op op, E l, E r) : op(op), left(std::move(l)), right(std::move(r)) {}
    Value eval(State& s) const override {
        // Left-to-right evaluation: both expressions can change the heap.
        std::int64_t a = integer(left->eval(s));
        std::int64_t b = integer(right->eval(s));
        switch (op) {
            case Op::Add: return a + b;
            case Op::Sub: return a - b;
            case Op::Mul: return a * b;
            case Op::Le:  return a <= b;
            case Op::Eq:  return a == b;
        }
        throw std::runtime_error("unknown binary operator");
    }
};
struct New : Expr {
    E initial;
    explicit New(E e) : initial(std::move(e)) {}
    Value eval(State& s) const override { return s.allocate(integer(initial->eval(s))); }
};
struct Deref : Expr {
    E pointer;
    explicit Deref(E e) : pointer(std::move(e)) {}
    Value eval(State& s) const override { return s.load(reference(pointer->eval(s))); }
};

struct Stmt {
    virtual ~Stmt() = default;
    virtual void exec(State& state) const = 0;
};
using S = std::shared_ptr<const Stmt>;
struct Let : Stmt {
    std::string name; E rhs;
    Let(std::string n, E e) : name(std::move(n)), rhs(std::move(e)) {}
    void exec(State& state) const override { state.declare(name, rhs->eval(state)); }
};
struct Assign : Stmt {
    std::string name; E rhs;
    Assign(std::string n, E e) : name(std::move(n)), rhs(std::move(e)) {}
    void exec(State& state) const override { state.assign(name, rhs->eval(state)); }
};
struct Write : Stmt {
    E pointer, rhs;
    Write(E p, E e) : pointer(std::move(p)), rhs(std::move(e)) {}
    void exec(State& state) const override {
        Ref r = reference(pointer->eval(state));
        std::int64_t n = integer(rhs->eval(state));
        state.store(r, n);
    }
};
struct Block : Stmt {
    std::vector<S> statements;
    explicit Block(std::vector<S> xs) : statements(std::move(xs)) {}
    void exec(State& state) const override {
        state.scopes.emplace_back();
        for (const auto& stmt : statements) stmt->exec(state);
        state.scopes.pop_back();
    }
};
struct If : Stmt {
    E cond; S yes, no;
    If(E c, S y, S n) : cond(std::move(c)), yes(std::move(y)), no(std::move(n)) {}
    void exec(State& s) const override { (boolean(cond->eval(s)) ? yes : no)->exec(s); }
};
struct While : Stmt {
    E cond; S body;
    While(E c, S b) : cond(std::move(c)), body(std::move(b)) {}
    void exec(State& s) const override { while (boolean(cond->eval(s))) body->exec(s); }
};

template <typename T, typename... Args>
std::shared_ptr<const T> node(Args&&... args) {
    return std::make_shared<const T>(std::forward<Args>(args)...);
}
E I(std::int64_t n) { return node<IntLit>(n); }
E V(const std::string& n) { return node<Variable>(n); }
E bin(Op o, E a, E b) { return node<Binary>(o, std::move(a), std::move(b)); }
E plus(E a, E b) { return bin(Op::Add, a, b); }
E minus(E a, E b) { return bin(Op::Sub, a, b); }
E times(E a, E b) { return bin(Op::Mul, a, b); }
E le(E a, E b) { return bin(Op::Le, a, b); }

struct Program { std::vector<std::string> inputs; std::vector<S> body; E result; };

Program add_program() {
    return {{"m", "n"}, {}, plus(V("m"), V("n"))};
}
Program half_program() {
    return {{"n"}, {
        node<Let>("result", I(0)), node<Let>("remaining", V("n")),
        node<While>(le(I(2), V("remaining")), node<Block>(std::vector<S>{
            node<Assign>("remaining", minus(V("remaining"), I(2))),
            node<Assign>("result", plus(V("result"), I(1)))
        }))
    }, V("result")};
}
Program factorial_program() {
    return {{"n"}, {
        node<Let>("result", I(1)), node<Let>("i", I(1)),
        node<While>(le(V("i"), V("n")), node<Block>(std::vector<S>{
            node<Assign>("result", times(V("result"), V("i"))),
            node<Assign>("i", plus(V("i"), I(1)))
        }))
    }, V("result")};
}
Program fibonacci_program() {
    return {{"n"}, {
        node<Let>("a", I(0)), node<Let>("b", I(1)), node<Let>("i", I(0)),
        node<While>(le(plus(V("i"), I(1)), V("n")), node<Block>(std::vector<S>{
            node<Let>("next", plus(V("a"), V("b"))),
            node<Assign>("a", V("b")),
            node<Assign>("b", V("next")),
            node<Assign>("i", plus(V("i"), I(1)))
        }))
    }, V("a")};
}
Program alias_program() {
    return {{"m", "n"}, {
        node<Let>("p", node<New>(V("m"))),
        node<Let>("q", V("p")),
        node<Write>(V("q"), plus(node<Deref>(V("q")), V("n")))
    }, node<Deref>(V("p"))};
}

std::int64_t run(const Program& program, const std::vector<std::int64_t>& args) {
    if (program.inputs.size() != args.size()) throw std::runtime_error("wrong number of inputs");
    State state;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] < 0) throw std::runtime_error("inputs must be natural numbers");
        state.declare(program.inputs[i], args[i]);
    }
    for (const auto& stmt : program.body) stmt->exec(state);
    return integer(program.result->eval(state));
}

int main(int argc, char** argv) {
    try {
        if (argc < 2) throw std::runtime_error("usage: refimp-ast <add|half|factorial|fibonacci|alias> [natural-number inputs]");
        std::string selection = argv[1];
        Program program;
        if (selection == "add") program = add_program();
        else if (selection == "half") program = half_program();
        else if (selection == "factorial") program = factorial_program();
        else if (selection == "fibonacci") program = fibonacci_program();
        else if (selection == "alias") program = alias_program();
        else throw std::runtime_error("unknown demo: " + selection);
        std::vector<std::int64_t> args;
        for (int i = 2; i < argc; ++i) {
            std::string text = argv[i];
            std::size_t used = 0;
            std::int64_t n = std::stoll(text, &used, 10);
            if (used != text.size()) throw std::runtime_error("input is not an integer: " + text);
            args.push_back(n);
        }
        std::cout << run(program, args) << '\n';
    } catch (const std::exception& ex) {
        std::cerr << "RefIMP error: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}
