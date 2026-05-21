#pragma once

#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace nwawe {

struct SourceLocation {
    std::size_t line = 1;
    std::size_t column = 1;
};

enum class TokenKind {
    EndOfFile,
    Identifier,
    Number,
    String,
    Arrow,
    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    LAngle,
    RAngle,
    Comma,
    Colon,
    Semicolon,
    Plus,
    Minus,
    Star,
    Slash,
    Caret,
    Underscore,
    Equal,
    Apostrophe,
    Backslash,
    Exclamation,
};

struct Token {
    TokenKind kind{};
    std::string lexeme;
    double numberValue = 0.0;
    SourceLocation location{};
};

class Lexer {
public:
    explicit Lexer(std::string source);
    std::vector<Token> lex();

private:
    std::string source_;
    std::size_t index_ = 0;
    SourceLocation location_{};

    char peek(std::size_t offset = 0) const;
    char advance();
    bool isAtEnd() const;
    void skipWhitespaceAndComments();
    bool isInlineCommentStart() const;
    Token makeToken(TokenKind kind, std::string lexeme = {}, double numberValue = 0.0) const;
};

struct Expr;
struct Stmt;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

struct Expr {
    enum class Kind {
        Number,
        String,
        Identifier,
        Unary,
        Binary,
        Call,
    };

    Kind kind;
    SourceLocation location{};
    double numberValue = 0.0;
    std::string text;
    char op = 0;
    ExprPtr left;
    ExprPtr right;
    std::vector<ExprPtr> arguments;
};

struct Stmt {
    enum class Kind {
        Expression,
        Assignment,
        Capture,
        FunctionDef,
        Return,
        Block,
    };

    Kind kind;
    SourceLocation location{};
    std::string name;
    ExprPtr expression;
    std::vector<std::string> parameters;
    std::vector<StmtPtr> body;
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    std::vector<StmtPtr> parseProgram();

private:
    std::vector<Token> tokens_;
    std::size_t current_ = 0;

    const Token& peek(std::size_t offset = 0) const;
    bool isAtEnd() const;
    bool check(TokenKind kind, std::size_t offset = 0) const;
    bool match(std::initializer_list<TokenKind> kinds);
    const Token& advance();
    const Token& previous() const;
    const Token& consume(TokenKind kind, std::string_view message);

    StmtPtr parseStatement();
    StmtPtr parseBlockStatement();
    StmtPtr parseFunctionDef(const Token& nameToken);
    StmtPtr parseReturnStatement();
    StmtPtr parseAssignmentOrExpression();
    StmtPtr parseCaptureStatement(ExprPtr sourceExpression = nullptr);
    std::vector<std::string> parseParameterList();
    std::vector<ExprPtr> parseArgumentList(TokenKind closingKind, bool textFriendly);

    ExprPtr parseExpression();
    ExprPtr parseAdditive();
    ExprPtr parseMultiplicative();
    ExprPtr parsePower();
    ExprPtr parseUnary();
    ExprPtr parseCall();
    ExprPtr parsePrimary();

    bool isStatementBoundary() const;
};

class Interpreter {
public:
    struct Value {
        using Storage = std::variant<std::monostate, double, std::string>;
        Storage storage;

        Value() = default;
        Value(double value);
        Value(std::string value);

        bool isNull() const;
        bool isNumber() const;
        bool isString() const;
        double asNumber() const;
        const std::string& asString() const;
    };

    using NativeFunction = std::function<Value(const std::vector<Value>&, Interpreter&)>;

    struct Function {
        std::vector<std::string> parameters;
        const std::vector<StmtPtr>* body = nullptr;
    };

    Interpreter();

    void registerNative(std::string name, NativeFunction function);
    void execute(const std::vector<StmtPtr>& program);
    void executeStatement(const Stmt& statement);
    Value evaluate(const Expr& expression);
    void setInput(std::istream* input);
    void setOutput(std::ostream* output);
    const std::string& output() const;
    std::string takeOutput();

private:
    std::unordered_map<std::string, Value> globals_;
    std::unordered_map<std::string, Function> userFunctions_;
    std::unordered_map<std::string, NativeFunction> nativeFunctions_;
    std::istream* input_ = nullptr;
    std::ostream* output_ = nullptr;
    std::string outputBuffer_;
    std::vector<std::unordered_map<std::string, Value>> scopes_;
    bool returning_ = false;
    Value returnValue_;
    Value lastValue_;

    Value getVariable(const std::string& name) const;
    void setVariable(const std::string& name, Value value);
    void pushScope();
    void popScope();
    void appendOutput(const std::string& text);
    Value callFunction(const std::string& name, const std::vector<Value>& arguments);
    Value executeFunctionBody(const Function& function, const std::vector<Value>& arguments);
    Value evaluate(const Expr& expression, bool textMode);
    Value evaluateBinary(char op, const Value& left, const Value& right);
    Value evaluateUnary(char op, const Value& operand);
    std::string stringify(const Value& value) const;
};

std::vector<StmtPtr> parseSource(const std::string& source);
int runSource(const std::string& source, std::istream& input, std::ostream& output);

} // namespace nwawe
