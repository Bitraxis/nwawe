#include "nwawe/runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nwawe {
namespace {

struct ParseError : std::runtime_error {
    explicit ParseError(const std::string& message)
        : std::runtime_error(message) {}
};

std::string makeMessage(const Token& token, const std::string& message) {
    std::ostringstream stream;
    stream << "line " << token.location.line << ", column " << token.location.column << ": " << message;
    return stream.str();
}

bool isIdentifierStart(char ch) {
    return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

bool isIdentifierPart(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '!';
}

std::string formatNumber(double value) {
    std::ostringstream stream;
    stream << std::setprecision(15) << value;
    std::string text = stream.str();
    if (text.find('.') != std::string::npos) {
        while (!text.empty() && text.back() == '0') {
            text.pop_back();
        }
        if (!text.empty() && text.back() == '.') {
            text.pop_back();
        }
    }
    if (text == "-0") {
        text = "0";
    }
    return text;
}

bool isNumericValue(const Interpreter::Value& value) {
    return std::holds_alternative<double>(value.storage);
}

} // namespace

Lexer::Lexer(std::string source)
    : source_(std::move(source)) {}

char Lexer::peek(std::size_t offset) const {
    if (index_ + offset >= source_.size()) {
        return '\0';
    }
    return source_[index_ + offset];
}

char Lexer::advance() {
    if (isAtEnd()) {
        return '\0';
    }

    const char ch = source_[index_++];
    if (ch == '\n') {
        ++location_.line;
        location_.column = 1;
    } else {
        ++location_.column;
    }
    return ch;
}

bool Lexer::isAtEnd() const {
    return index_ >= source_.size();
}

bool Lexer::isInlineCommentStart() const {
    if (index_ == 0) {
        return true;
    }

    std::size_t cursor = index_;
    while (cursor > 0) {
        const char previous = source_[cursor - 1];
        if (previous == ' ' || previous == '\t' || previous == '\r') {
            --cursor;
            continue;
        }

        return previous == ')' || previous == ']' || previous == '}' || previous == '>' || previous == ';';
    }

    return true;
}

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        const char ch = peek();
        if (ch == ' ' || ch == '\t' || ch == '\r') {
            advance();
            continue;
        }

        if (ch == '/' && (peek(1) == '/' || location_.column == 1 || isInlineCommentStart())) {
            while (!isAtEnd() && peek() != '\n') {
                advance();
            }
            continue;
        }

        break;
    }
}

Token Lexer::makeToken(TokenKind kind, std::string lexeme, double numberValue) const {
    return Token{kind, std::move(lexeme), numberValue, location_};
}

std::vector<Token> Lexer::lex() {
    std::vector<Token> tokens;

    while (!isAtEnd()) {
        skipWhitespaceAndComments();
        if (isAtEnd()) {
            break;
        }

        const SourceLocation start = location_;
        const char ch = peek();

        if (ch == '\n') {
            advance();
            tokens.push_back(Token{TokenKind::Semicolon, ";", 0.0, start});
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(ch))) {
            std::string number;
            while (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '.') {
                number.push_back(advance());
            }
            tokens.push_back(Token{TokenKind::Number, number, std::stod(number), start});
            continue;
        }

        if (ch == '"') {
            advance();
            std::string text;
            while (!isAtEnd() && peek() != '"') {
                if (peek() == '\\' && peek(1) != '\0') {
                    advance();
                    const char escaped = advance();
                    switch (escaped) {
                        case 'n': text.push_back('\n'); break;
                        case 't': text.push_back('\t'); break;
                        case 'r': text.push_back('\r'); break;
                        case '"': text.push_back('"'); break;
                        case '\\': text.push_back('\\'); break;
                        default: text.push_back(escaped); break;
                    }
                    continue;
                }
                text.push_back(advance());
            }
            if (isAtEnd()) {
                throw ParseError(makeMessage(Token{TokenKind::String, {}, 0.0, start}, "unterminated string literal"));
            }
            advance();
            tokens.push_back(Token{TokenKind::String, std::move(text), 0.0, start});
            continue;
        }

        if (isIdentifierStart(ch)) {
            std::string text;
            while (isIdentifierPart(peek())) {
                text.push_back(advance());
            }
            tokens.push_back(Token{TokenKind::Identifier, std::move(text), 0.0, start});
            continue;
        }

        advance();
        switch (ch) {
            case '=':
                if (peek() == '>') {
                    advance();
                    tokens.push_back(Token{TokenKind::Arrow, "=>", 0.0, start});
                    break;
                }
                tokens.push_back(Token{TokenKind::Equal, "=", 0.0, start});
                break;
            case '(': tokens.push_back(Token{TokenKind::LParen, "(", 0.0, start}); break;
            case ')': tokens.push_back(Token{TokenKind::RParen, ")", 0.0, start}); break;
            case '{': tokens.push_back(Token{TokenKind::LBrace, "{", 0.0, start}); break;
            case '}': tokens.push_back(Token{TokenKind::RBrace, "}", 0.0, start}); break;
            case '[': tokens.push_back(Token{TokenKind::LBracket, "[", 0.0, start}); break;
            case ']': tokens.push_back(Token{TokenKind::RBracket, "]", 0.0, start}); break;
            case '<': tokens.push_back(Token{TokenKind::LAngle, "<", 0.0, start}); break;
            case '>': tokens.push_back(Token{TokenKind::RAngle, ">", 0.0, start}); break;
            case ',': tokens.push_back(Token{TokenKind::Comma, ",", 0.0, start}); break;
            case ':': tokens.push_back(Token{TokenKind::Colon, ":", 0.0, start}); break;
            case ';': tokens.push_back(Token{TokenKind::Semicolon, ";", 0.0, start}); break;
            case '+': tokens.push_back(Token{TokenKind::Plus, "+", 0.0, start}); break;
            case '-': tokens.push_back(Token{TokenKind::Minus, "-", 0.0, start}); break;
            case '*': tokens.push_back(Token{TokenKind::Star, "*", 0.0, start}); break;
            case '/': tokens.push_back(Token{TokenKind::Slash, "/", 0.0, start}); break;
            case '^': tokens.push_back(Token{TokenKind::Caret, "^", 0.0, start}); break;
            case '_': tokens.push_back(Token{TokenKind::Underscore, "_", 0.0, start}); break;
            case '\'': tokens.push_back(Token{TokenKind::Apostrophe, "'", 0.0, start}); break;
            case '\\': tokens.push_back(Token{TokenKind::Backslash, "\\", 0.0, start}); break;
            case '!': tokens.push_back(Token{TokenKind::Exclamation, "!", 0.0, start}); break;
            default:
                throw ParseError(makeMessage(Token{TokenKind::EndOfFile, std::string(1, ch), 0.0, start}, "unexpected character"));
        }
    }

    tokens.push_back(Token{TokenKind::EndOfFile, {}, 0.0, location_});
    return tokens;
}

Parser::Parser(std::vector<Token> tokens)
    : tokens_(std::move(tokens)) {}

const Token& Parser::peek(std::size_t offset) const {
    const std::size_t index = std::min(current_ + offset, tokens_.size() - 1);
    return tokens_[index];
}

bool Parser::isAtEnd() const {
    return peek().kind == TokenKind::EndOfFile;
}

bool Parser::check(TokenKind kind, std::size_t offset) const {
    return peek(offset).kind == kind;
}

bool Parser::match(std::initializer_list<TokenKind> kinds) {
    for (TokenKind kind : kinds) {
        if (check(kind)) {
            advance();
            return true;
        }
    }
    return false;
}

const Token& Parser::advance() {
    if (!isAtEnd()) {
        ++current_;
    }
    return previous();
}

const Token& Parser::previous() const {
    return tokens_[current_ - 1];
}

const Token& Parser::consume(TokenKind kind, std::string_view message) {
    if (check(kind)) {
        return advance();
    }
    throw ParseError(makeMessage(peek(), std::string(message)));
}

bool Parser::isStatementBoundary() const {
    return check(TokenKind::Semicolon) || check(TokenKind::RBrace) || check(TokenKind::EndOfFile);
}

std::vector<std::string> Parser::parseParameterList() {
    std::vector<std::string> parameters;

    while (!check(TokenKind::Colon) && !check(TokenKind::RBrace) && !isAtEnd()) {
        if (match({TokenKind::Comma, TokenKind::Semicolon})) {
            continue;
        }

        if (check(TokenKind::Identifier)) {
            const Token& nameToken = advance();
            parameters.push_back(nameToken.lexeme);
            if (check(TokenKind::RAngle)) {
                advance();
            }
            continue;
        }

        if (check(TokenKind::LAngle)) {
            advance();
            continue;
        }

        break;
    }

    return parameters;
}

std::vector<ExprPtr> Parser::parseArgumentList(TokenKind closingKind, bool textFriendly) {
    std::vector<ExprPtr> arguments;

    while (!check(closingKind) && !isAtEnd()) {
        if (match({TokenKind::Comma, TokenKind::Semicolon})) {
            continue;
        }

        if (textFriendly) {
            const std::size_t start = current_;
            bool parsedAsExpression = false;
            try {
                auto expression = parseExpression();
                if (check(closingKind) || check(TokenKind::Comma) || check(TokenKind::Semicolon)) {
                    arguments.push_back(std::move(expression));
                    parsedAsExpression = true;
                }
            } catch (const ParseError&) {
                parsedAsExpression = false;
            }

            if (!parsedAsExpression) {
                current_ = start;
                std::string text;
                TokenKind previousKind = TokenKind::EndOfFile;
                while (!check(closingKind) && !check(TokenKind::Comma) && !check(TokenKind::Semicolon) && !isAtEnd()) {
                    const Token& token = advance();
                    const bool currentIsWord = token.kind == TokenKind::Identifier || token.kind == TokenKind::Number || token.kind == TokenKind::String;
                    const bool previousIsWord = previousKind == TokenKind::Identifier || previousKind == TokenKind::Number || previousKind == TokenKind::String;
                    if (!text.empty() && currentIsWord && previousIsWord) {
                        text.push_back(' ');
                    }
                    text += token.lexeme;
                    previousKind = token.kind;
                }
                auto literal = std::make_unique<Expr>();
                literal->kind = Expr::Kind::String;
                literal->location = peek().location;
                literal->text = std::move(text);
                arguments.push_back(std::move(literal));
            }
            continue;
        }

        arguments.push_back(parseExpression());

        if (check(closingKind)) {
            break;
        }
        match({TokenKind::Comma, TokenKind::Semicolon});
    }

    return arguments;
}

StmtPtr Parser::parseReturnStatement() {
    const Token& start = consume(TokenKind::LBracket, "expected '[' to start return statement");
    match({TokenKind::Backslash});
    auto statement = std::make_unique<Stmt>();
    statement->kind = Stmt::Kind::Return;
    statement->location = start.location;
    statement->expression = parseExpression();
    while (match({TokenKind::Semicolon, TokenKind::Comma})) {
        statement->expression = parseExpression();
    }
    consume(TokenKind::RBracket, "expected ']' to close return statement");
    return statement;
}

StmtPtr Parser::parseCaptureStatement(ExprPtr sourceExpression) {
    const Token& start = consume(TokenKind::Arrow, "expected '=>' to start capture statement");
    std::string name;
    if (match({TokenKind::LParen})) {
        const Token& nameToken = consume(TokenKind::Identifier, "expected identifier inside capture");
        consume(TokenKind::RParen, "expected ')' after capture target");
        name = nameToken.lexeme;
    } else {
        const Token& nameToken = consume(TokenKind::Identifier, "expected identifier after '=>' ");
        name = nameToken.lexeme;
    }

    auto statement = std::make_unique<Stmt>();
    statement->kind = Stmt::Kind::Capture;
    statement->location = start.location;
    statement->name = std::move(name);
    statement->expression = std::move(sourceExpression);
    return statement;
}

StmtPtr Parser::parseFunctionDef(const Token& nameToken) {
    auto statement = std::make_unique<Stmt>();
    statement->kind = Stmt::Kind::FunctionDef;
    statement->location = nameToken.location;
    statement->name = nameToken.lexeme;

    consume(TokenKind::LBrace, "expected '{' after function name");
    if (match({TokenKind::LAngle})) {
        statement->parameters = parseParameterList();
        if (match({TokenKind::Colon})) {
            // Optional separator before the body.
        }
    }

    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        if (match({TokenKind::Semicolon})) {
            continue;
        }
        statement->body.push_back(parseStatement());
    }

    consume(TokenKind::RBrace, "expected '}' after function body");
    return statement;
}

StmtPtr Parser::parseAssignmentOrExpression() {
    if (check(TokenKind::Identifier) && check(TokenKind::Equal, 1)) {
        const Token nameToken = advance();
        consume(TokenKind::Equal, "expected '=' in assignment");
        auto statement = std::make_unique<Stmt>();
        statement->kind = Stmt::Kind::Assignment;
        statement->location = nameToken.location;
        statement->name = nameToken.lexeme;
        statement->expression = parseExpression();
        return statement;
    }

    auto statement = std::make_unique<Stmt>();
    statement->kind = Stmt::Kind::Expression;
    statement->location = peek().location;
    statement->expression = parseExpression();
    return statement;
}

StmtPtr Parser::parseBlockStatement() {
    auto statement = std::make_unique<Stmt>();
    statement->kind = Stmt::Kind::Block;
    statement->location = peek().location;
    consume(TokenKind::LBrace, "expected '{'");
    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        if (match({TokenKind::Semicolon})) {
            continue;
        }
        statement->body.push_back(parseStatement());
    }
    consume(TokenKind::RBrace, "expected '}' after block");
    return statement;
}

StmtPtr Parser::parseStatement() {
    while (match({TokenKind::Semicolon})) {
        // Skip separators.
    }

    if (isAtEnd() || check(TokenKind::RBrace)) {
        auto statement = std::make_unique<Stmt>();
        statement->kind = Stmt::Kind::Expression;
        statement->location = peek().location;
        return statement;
    }

    if (check(TokenKind::Arrow)) {
        auto statement = parseCaptureStatement();
        match({TokenKind::Semicolon});
        return statement;
    }

    if (check(TokenKind::LParen)) {
        advance();
        if (check(TokenKind::Identifier) && check(TokenKind::Equal, 1)) {
            auto statement = parseAssignmentOrExpression();
            consume(TokenKind::RParen, "expected ')' after parenthesized statement");
            return statement;
        }
        auto statement = parseAssignmentOrExpression();
        consume(TokenKind::RParen, "expected ')' after parenthesized expression");
        return statement;
    }

    if (check(TokenKind::LBracket)) {
        return parseReturnStatement();
    }

    if (check(TokenKind::Identifier) && check(TokenKind::LBrace, 1)) {
        const Token nameToken = advance();
        return parseFunctionDef(nameToken);
    }

    auto statement = parseAssignmentOrExpression();
    if (check(TokenKind::Arrow) && statement->kind == Stmt::Kind::Expression) {
        auto sourceExpression = std::move(statement->expression);
        return parseCaptureStatement(std::move(sourceExpression));
    }

    return statement;
}

ExprPtr Parser::parseExpression() {
    return parseAdditive();
}

ExprPtr Parser::parseAdditive() {
    auto expression = parseMultiplicative();
    while (match({TokenKind::Plus, TokenKind::Minus})) {
        const Token op = previous();
        auto right = parseMultiplicative();
        auto binary = std::make_unique<Expr>();
        binary->kind = Expr::Kind::Binary;
        binary->location = op.location;
        binary->op = op.lexeme.empty() ? 0 : op.lexeme[0];
        binary->left = std::move(expression);
        binary->right = std::move(right);
        expression = std::move(binary);
    }
    return expression;
}

ExprPtr Parser::parseMultiplicative() {
    auto expression = parsePower();
    while (match({TokenKind::Star, TokenKind::Slash, TokenKind::Underscore})) {
        const Token op = previous();
        auto right = parsePower();
        auto binary = std::make_unique<Expr>();
        binary->kind = Expr::Kind::Binary;
        binary->location = op.location;
        binary->op = op.lexeme.empty() ? 0 : op.lexeme[0];
        binary->left = std::move(expression);
        binary->right = std::move(right);
        expression = std::move(binary);
    }
    return expression;
}

ExprPtr Parser::parsePower() {
    auto expression = parseUnary();
    while (match({TokenKind::Caret})) {
        const Token op = previous();
        auto right = parseUnary();
        auto binary = std::make_unique<Expr>();
        binary->kind = Expr::Kind::Binary;
        binary->location = op.location;
        binary->op = '^';
        binary->left = std::move(expression);
        binary->right = std::move(right);
        expression = std::move(binary);
    }
    return expression;
}

ExprPtr Parser::parseUnary() {
    if (match({TokenKind::Minus, TokenKind::Plus, TokenKind::Star, TokenKind::Backslash})) {
        const Token op = previous();
        auto unary = std::make_unique<Expr>();
        unary->kind = Expr::Kind::Unary;
        unary->location = op.location;
        unary->op = op.lexeme.empty() ? 0 : op.lexeme[0];
        unary->right = parseUnary();
        return unary;
    }

    return parseCall();
}

ExprPtr Parser::parseCall() {
    auto expression = parsePrimary();

    while (true) {
        if (match({TokenKind::LAngle})) {
            const bool textFriendly = expression->kind == Expr::Kind::Identifier && (expression->text == "print" || expression->text == "input");
            auto arguments = parseArgumentList(TokenKind::RAngle, textFriendly);
            consume(TokenKind::RAngle, "expected '>' to close call arguments");

            if (expression->kind != Expr::Kind::Identifier && expression->kind != Expr::Kind::Call) {
                throw ParseError(makeMessage(previous(), "only named calls can use '<...>' syntax"));
            }

            if (expression->kind == Expr::Kind::Identifier) {
                auto call = std::make_unique<Expr>();
                call->kind = Expr::Kind::Call;
                call->location = expression->location;
                call->text = expression->text;
                call->arguments = std::move(arguments);
                expression = std::move(call);
            } else {
                auto& targetArguments = expression->arguments;
                targetArguments.insert(
                    targetArguments.end(),
                    std::make_move_iterator(arguments.begin()),
                    std::make_move_iterator(arguments.end()));
            }
            continue;
        }

        if (match({TokenKind::Apostrophe})) {
            const bool textFriendly = expression->kind == Expr::Kind::Identifier && (expression->text == "print" || expression->text == "input");
            auto arguments = parseArgumentList(TokenKind::Apostrophe, textFriendly);
            consume(TokenKind::Apostrophe, "expected '\'' to close call arguments");

            if (expression->kind == Expr::Kind::Identifier) {
                auto call = std::make_unique<Expr>();
                call->kind = Expr::Kind::Call;
                call->location = expression->location;
                call->text = expression->text;
                call->arguments = std::move(arguments);
                expression = std::move(call);
            } else if (expression->kind == Expr::Kind::Call) {
                auto& targetArguments = expression->arguments;
                targetArguments.insert(
                    targetArguments.end(),
                    std::make_move_iterator(arguments.begin()),
                    std::make_move_iterator(arguments.end()));
            } else {
                throw ParseError(makeMessage(previous(), "only named calls can use '\'' syntax"));
            }
            continue;
        }

        break;
    }

    return expression;
}

ExprPtr Parser::parsePrimary() {
    if (match({TokenKind::Number})) {
        auto expression = std::make_unique<Expr>();
        expression->kind = Expr::Kind::Number;
        expression->location = previous().location;
        expression->numberValue = previous().numberValue;
        return expression;
    }

    if (match({TokenKind::String})) {
        auto expression = std::make_unique<Expr>();
        expression->kind = Expr::Kind::String;
        expression->location = previous().location;
        expression->text = previous().lexeme;
        return expression;
    }

    if (match({TokenKind::Identifier})) {
        auto expression = std::make_unique<Expr>();
        expression->kind = Expr::Kind::Identifier;
        expression->location = previous().location;
        expression->text = previous().lexeme;
        return expression;
    }

    if (match({TokenKind::LParen})) {
        auto expression = parseExpression();
        consume(TokenKind::RParen, "expected ')' after expression");
        return expression;
    }

    if (match({TokenKind::LBracket})) {
        auto expression = parseExpression();
        consume(TokenKind::RBracket, "expected ']' after expression");
        return expression;
    }

    if (match({TokenKind::LAngle})) {
        auto expression = parseExpression();
        consume(TokenKind::RAngle, "expected '>' after angle-bracket expression");
        if (expression->kind == Expr::Kind::Identifier) {
            auto literal = std::make_unique<Expr>();
            literal->kind = Expr::Kind::String;
            literal->location = expression->location;
            literal->text = expression->text;
            return literal;
        }
        return expression;
    }

    throw ParseError(makeMessage(peek(), "expected an expression"));
}

std::vector<StmtPtr> Parser::parseProgram() {
    std::vector<StmtPtr> program;
    while (!isAtEnd()) {
        if (match({TokenKind::Semicolon})) {
            continue;
        }
        program.push_back(parseStatement());
        match({TokenKind::Semicolon});
    }
    return program;
}

Interpreter::Value::Value(double value)
    : storage(value) {}

Interpreter::Value::Value(std::string value)
    : storage(std::move(value)) {}

bool Interpreter::Value::isNull() const {
    return std::holds_alternative<std::monostate>(storage);
}

bool Interpreter::Value::isNumber() const {
    return std::holds_alternative<double>(storage);
}

bool Interpreter::Value::isString() const {
    return std::holds_alternative<std::string>(storage);
}

double Interpreter::Value::asNumber() const {
    if (!isNumber()) {
        throw std::runtime_error("expected number");
    }
    return std::get<double>(storage);
}

const std::string& Interpreter::Value::asString() const {
    if (!isString()) {
        throw std::runtime_error("expected string");
    }
    return std::get<std::string>(storage);
}

Interpreter::Interpreter() {
    scopes_.emplace_back();

    registerNative("print", [this](const std::vector<Value>& arguments, Interpreter&) {
        for (std::size_t index = 0; index < arguments.size(); ++index) {
            if (index > 0) {
                appendOutput(" ");
            }
            appendOutput(stringify(arguments[index]));
        }
        appendOutput("\n");
        return Value{};
    });

    registerNative("input", [this](const std::vector<Value>& arguments, Interpreter&) {
        if (!arguments.empty()) {
            appendOutput(stringify(arguments.front()));
        }
        if (!input_) {
            return Value{};
        }

        std::string line;
        std::getline(*input_, line);
        return Value{line};
    });

    auto registerMath = [this](const std::string& name, const std::function<double(double)>& fn) {
        registerNative(name, [fn](const std::vector<Value>& arguments, Interpreter&) {
            if (arguments.empty()) {
                throw std::runtime_error("missing argument for mathematical function");
            }
            return Value{fn(arguments.front().asNumber())};
        });
    };

    registerMath("sin", [](double value) { return std::sin(value); });
    registerMath("cos", [](double value) { return std::cos(value); });
    registerMath("tan", [](double value) { return std::tan(value); });
    registerMath("cot", [](double value) { return 1.0 / std::tan(value); });
    registerMath("log", [](double value) { return std::log(value); });
}

void Interpreter::registerNative(std::string name, NativeFunction function) {
    nativeFunctions_[std::move(name)] = std::move(function);
}

void Interpreter::setInput(std::istream* input) {
    input_ = input;
}

void Interpreter::setOutput(std::ostream* output) {
    output_ = output;
}

const std::string& Interpreter::output() const {
    return outputBuffer_;
}

std::string Interpreter::takeOutput() {
    std::string captured = std::move(outputBuffer_);
    outputBuffer_.clear();
    return captured;
}

Interpreter::Value Interpreter::getVariable(const std::string& name) const {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        const auto found = scope->find(name);
        if (found != scope->end()) {
            return found->second;
        }
    }

    const auto global = globals_.find(name);
    if (global != globals_.end()) {
        return global->second;
    }

    throw std::runtime_error("unknown variable: " + name);
}

void Interpreter::setVariable(const std::string& name, Value value) {
    if (scopes_.empty()) {
        scopes_.emplace_back();
    }
    scopes_.back()[name] = value;
    if (scopes_.size() == 1) {
        globals_[name] = value;
    }
}

void Interpreter::pushScope() {
    scopes_.emplace_back();
}

void Interpreter::popScope() {
    if (scopes_.size() > 1) {
        scopes_.pop_back();
    }
}

void Interpreter::appendOutput(const std::string& text) {
    outputBuffer_ += text;
    if (output_) {
        (*output_) << text;
        output_->flush();
    }
}

std::string Interpreter::stringify(const Value& value) const {
    if (value.isNull()) {
        return {};
    }
    if (value.isNumber()) {
        return formatNumber(value.asNumber());
    }
    return value.asString();
}

Interpreter::Value Interpreter::evaluateBinary(char op, const Value& left, const Value& right) {
    switch (op) {
        case '+':
            if (left.isString() || right.isString()) {
                return Value{stringify(left) + stringify(right)};
            }
            return Value{left.asNumber() + right.asNumber()};
        case '-':
            return Value{left.asNumber() - right.asNumber()};
        case '*':
            return Value{left.asNumber() * right.asNumber()};
        case '/':
        case '_': {
            const double divisor = right.asNumber();
            if (divisor == 0.0) {
                throw std::runtime_error("division by zero");
            }
            return Value{left.asNumber() / divisor};
        }
        case '^':
            return Value{std::pow(left.asNumber(), right.asNumber())};
        default:
            throw std::runtime_error("unsupported operator");
    }
}

Interpreter::Value Interpreter::evaluateUnary(char op, const Value& operand) {
    switch (op) {
        case '+':
            return operand;
        case '-':
            return Value{-operand.asNumber()};
        case '*':
            return operand;
        case '\\':
            return operand;
        default:
            throw std::runtime_error("unsupported unary operator");
    }
}

Interpreter::Value Interpreter::callFunction(const std::string& name, const std::vector<Value>& arguments) {
    if (const auto native = nativeFunctions_.find(name); native != nativeFunctions_.end()) {
        return native->second(arguments, *this);
    }

    if (const auto user = userFunctions_.find(name); user != userFunctions_.end()) {
        return executeFunctionBody(user->second, arguments);
    }

    throw std::runtime_error("unknown function: " + name);
}

Interpreter::Value Interpreter::executeFunctionBody(const Function& function, const std::vector<Value>& arguments) {
    const bool savedReturning = returning_;
    const Value savedReturnValue = returnValue_;
    const Value savedLastValue = lastValue_;
    returning_ = false;
    returnValue_ = Value{};
    lastValue_ = Value{};

    pushScope();
    for (std::size_t index = 0; index < function.parameters.size(); ++index) {
        if (index < arguments.size()) {
            setVariable(function.parameters[index], arguments[index]);
        } else {
            setVariable(function.parameters[index], Value{});
        }
    }

    const std::vector<StmtPtr>* body = function.body;
    try {
        Value lastValue{};
        if (body != nullptr) {
            for (const auto& statement : *body) {
                if (returning_) {
                    break;
                }
                if (!statement) {
                    continue;
                }
                switch (statement->kind) {
                    case Stmt::Kind::Expression:
                        if (statement->expression) {
                            lastValue = evaluate(*statement->expression, false);
                            lastValue_ = lastValue;
                        }
                        break;
                    case Stmt::Kind::Assignment:
                        if (statement->expression) {
                            lastValue = evaluate(*statement->expression, false);
                            setVariable(statement->name, lastValue);
                            lastValue_ = lastValue;
                        }
                        break;
                    case Stmt::Kind::Capture:
                        if (statement->expression) {
                            lastValue = evaluate(*statement->expression, false);
                            lastValue_ = lastValue;
                        }
                        setVariable(statement->name, lastValue_);
                        break;
                    case Stmt::Kind::FunctionDef:
                        userFunctions_[statement->name] = Function{statement->parameters, &statement->body};
                        break;
                    case Stmt::Kind::Return:
                        returning_ = true;
                        returnValue_ = statement->expression ? evaluate(*statement->expression, false) : Value{};
                        lastValue_ = returnValue_;
                        break;
                    case Stmt::Kind::Block:
                        for (const auto& nested : statement->body) {
                            if (returning_) {
                                break;
                            }
                            if (nested) {
                                executeStatement(*nested);
                            }
                        }
                        break;
                }
            }
        }
        popScope();
        const Value result = returning_ ? returnValue_ : lastValue;
        returning_ = savedReturning;
        returnValue_ = savedReturnValue;
        lastValue_ = savedLastValue;
        return result;
    } catch (...) {
        popScope();
        returning_ = savedReturning;
        returnValue_ = savedReturnValue;
        lastValue_ = savedLastValue;
        throw;
    }
}

Interpreter::Value Interpreter::evaluate(const Expr& expression) {
    return evaluate(expression, false);
}

Interpreter::Value Interpreter::evaluate(const Expr& expression, bool textMode) {
    switch (expression.kind) {
        case Expr::Kind::Number:
            return Value{expression.numberValue};
        case Expr::Kind::String:
            return Value{expression.text};
        case Expr::Kind::Identifier:
            if (textMode) {
                return Value{expression.text};
            }
            return getVariable(expression.text);
        case Expr::Kind::Unary: {
            if (!expression.right) {
                return Value{};
            }
            if (expression.op == '*') {
                if (expression.right->kind == Expr::Kind::Identifier) {
                    return getVariable(expression.right->text);
                }
            }
            return evaluateUnary(expression.op, evaluate(*expression.right, false));
        }
        case Expr::Kind::Binary:
            if (expression.op == '>') {
                (void)evaluate(*expression.left, false);
                return evaluate(*expression.right, false);
            }
            return evaluateBinary(expression.op, evaluate(*expression.left, textMode), evaluate(*expression.right, textMode));
        case Expr::Kind::Call: {
            std::vector<Value> arguments;
            arguments.reserve(expression.arguments.size());
            const bool callUsesTextMode = expression.text == "print" || expression.text == "input";
            for (const auto& argument : expression.arguments) {
                arguments.push_back(evaluate(*argument, callUsesTextMode));
            }
            return callFunction(expression.text, arguments);
        }
    }

    return Value{};
}

void Interpreter::executeStatement(const Stmt& statement) {
    switch (statement.kind) {
        case Stmt::Kind::Expression:
            if (statement.expression) {
                lastValue_ = evaluate(*statement.expression, false);
            }
            break;
        case Stmt::Kind::Assignment:
            if (statement.expression) {
                lastValue_ = evaluate(*statement.expression, false);
                setVariable(statement.name, lastValue_);
            }
            break;
        case Stmt::Kind::Capture:
            if (statement.expression) {
                lastValue_ = evaluate(*statement.expression, false);
            }
            setVariable(statement.name, lastValue_);
            break;
        case Stmt::Kind::FunctionDef:
            userFunctions_[statement.name] = Function{statement.parameters, &statement.body};
            break;
        case Stmt::Kind::Return:
            returning_ = true;
            returnValue_ = statement.expression ? evaluate(*statement.expression, false) : Value{};
            lastValue_ = returnValue_;
            break;
        case Stmt::Kind::Block:
            pushScope();
            try {
                for (const auto& nested : statement.body) {
                    if (returning_) {
                        break;
                    }
                    if (nested) {
                        executeStatement(*nested);
                    }
                }
                popScope();
            } catch (...) {
                popScope();
                throw;
            }
            break;
    }
}

void Interpreter::execute(const std::vector<StmtPtr>& program) {
    for (const auto& statement : program) {
        if (returning_) {
            break;
        }
        if (statement) {
            executeStatement(*statement);
        }
    }
}

std::vector<StmtPtr> parseSource(const std::string& source) {
    Lexer lexer(source);
    Parser parser(lexer.lex());
    return parser.parseProgram();
}

int runSource(const std::string& source, std::istream& input, std::ostream& output) {
    Interpreter interpreter;
    interpreter.setInput(&input);
    interpreter.setOutput(&output);

    const auto program = parseSource(source);
    interpreter.execute(program);
    return 0;
}

} // namespace nwawe
