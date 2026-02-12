#include "agenui_http_agent.h"

#include <nlohmann/json.hpp>
#include <set>

#include "core/agenui_subscriber.h"
#include "core/agenui_uuid.h"

namespace agenui {

// Builder Implementation

HttpAgent::Builder::Builder() : _timeout(30) {}

HttpAgent::Builder& HttpAgent::Builder::withUrl(const std::string& url) {
    _url = url;
    return *this;
}

HttpAgent::Builder& HttpAgent::Builder::withHeader(const std::string& name, const std::string& value) {
    _headers[name] = value;
    return *this;
}

HttpAgent::Builder& HttpAgent::Builder::withBearerToken(const std::string& token) {
    _headers["Authorization"] = "Bearer " + token;
    return *this;
}

HttpAgent::Builder& HttpAgent::Builder::withTimeout(uint32_t seconds) {
    _timeout = seconds;
    return *this;
}

HttpAgent::Builder& HttpAgent::Builder::withAgentId(const AgentId& id) {
    _agentId = id;
    return *this;
}

HttpAgent::Builder& HttpAgent::Builder::withInitialMessages(const std::vector<Message>& messages) {
    _initialMessages = messages;
    return *this;
}

HttpAgent::Builder& HttpAgent::Builder::withInitialState(const nlohmann::json& state) {
    _initialState = state;
    return *this;
}

std::unique_ptr<HttpAgent> HttpAgent::Builder::build() {
    if (_url.empty()) {
        printf("[AGenUI][ERROR] Base URL is required\n");
        return nullptr;
    }

    // Set default Content-Type
    if (_headers.find("Content-Type") == _headers.end()) {
        _headers["Content-Type"] = "application/json";
    }

    return std::unique_ptr<HttpAgent>(new HttpAgent(_url, _headers, _agentId, _initialMessages, _initialState));
}

HttpAgent::Builder HttpAgent::builder() {
    return Builder();
}

// HttpAgent Implementation

HttpAgent::HttpAgent(const std::string& baseUrl, const std::map<std::string, std::string>& headers,
                     const AgentId& agentId, const std::vector<Message>& initialMessages,
                     const std::string& initialState)
    : _baseUrl(baseUrl), _headers(headers), _agentId(agentId) {
    _httpService = std::unique_ptr<HttpService>(new HttpService());
    _sseParser = std::unique_ptr<SseParser>(new SseParser());

    // Create persistent EventHandler
    RunAgentInput dummyInput;
    _eventHandler = std::make_shared<EventHandler>(initialMessages, initialState, dummyInput,
                                                    std::vector<std::shared_ptr<IAgentSubscriber>>());

    printf("[AGenUI][INFO] HttpAgent created with %zu initial messages\n", initialMessages.size());
}

HttpAgent::~HttpAgent() {}

AgentId HttpAgent::agentId() const {
    return _agentId;
}

// State access (delegated to EventHandler)

const std::vector<Message>& HttpAgent::messages() const {
    return _eventHandler->messages();
}

const std::string& HttpAgent::state() const {
    return _eventHandler->state();
}

// State modification (delegated to EventHandler)

void HttpAgent::addMessage(const Message& message) {
    // Get current messages and add new message
    auto msgs = _eventHandler->messages();
    msgs.push_back(message);

    // Create and apply mutation
    AgentStateMutation mutation;
    mutation.withMessages(msgs);
    _eventHandler->applyMutation(mutation);

    printf("[AGenUI][INFO] Message added, total messages: %zu\n", msgs.size());
}

void HttpAgent::setMessages(const std::vector<Message>& messages) {
    AgentStateMutation mutation;
    mutation.withMessages(messages);
    _eventHandler->applyMutation(mutation);

    printf("[AGenUI][INFO] Messages set, total messages: %zu\n", messages.size());
}

void HttpAgent::setState(const nlohmann::json& state) {
    AgentStateMutation mutation;
    mutation.withState(state);
    _eventHandler->applyMutation(mutation);

    printf("[AGenUI][INFO] State updated\n");
}

// Subscriber management (delegated to EventHandler)

void HttpAgent::subscribe(std::shared_ptr<IAgentSubscriber> subscriber) {
    _eventHandler->addSubscriber(subscriber);
    printf("[AGenUI][INFO] Subscriber added\n");
}

void HttpAgent::unsubscribe(std::shared_ptr<IAgentSubscriber> subscriber) {
    _eventHandler->removeSubscriber(subscriber);
    printf("[AGenUI][INFO] Subscriber removed\n");
}

void HttpAgent::clearSubscribers() {
    _eventHandler->clearSubscribers();
    printf("[AGenUI][INFO] All subscribers cleared\n");
}

// Middleware management

HttpAgent& HttpAgent::use(std::shared_ptr<IMiddleware> middleware) {
    _middlewareChain.addMiddleware(middleware);
    printf("[AGenUI][INFO] Middleware added, total: %zu\n", _middlewareChain.size());
    return *this;
}

MiddlewareChain& HttpAgent::middlewareChain() {
    return _middlewareChain;
}

// runAgent implementation

void HttpAgent::runAgent(const RunAgentParams& params, AgentSuccessCallback onSuccess, AgentErrorCallback onError) {
    printf("[AGenUI][INFO] Starting agent run\n");

    // 1. Build RunAgentInput with current messages and state
    RunAgentInput input;
    input.threadId = params.threadId.empty() ? UuidGenerator::generate() : params.threadId;
    input.runId = params.runId.empty() ? UuidGenerator::generate() : params.runId;
    input.state = _eventHandler->state();
    input.messages = _eventHandler->messages();
    input.tools = params.tools;
    input.context = params.context;
    input.forwardedProps = params.forwardedProps;

    printf("[AGenUI][DEBUG] Thread ID: %s\n", input.threadId.c_str());
    printf("[AGenUI][DEBUG] Run ID: %s\n", input.runId.c_str());
    printf("[AGenUI][DEBUG] Messages count: %zu\n", input.messages.size());

    // 2. Process request through middleware
    MiddlewareContext middlewareContext(&input, nullptr);
    middlewareContext.currentMessages = &_eventHandler->messages();
    middlewareContext.currentState = &_eventHandler->state();
    
    if (_middlewareChain.size() > 0) {
        printf("[AGenUI][INFO] Processing request through %zu middlewares\n", _middlewareChain.size());
        input = _middlewareChain.processRequest(input, middlewareContext);
        
        // Check if should continue
        if (!middlewareContext.shouldContinue) {
            printf("[AGenUI][INFO] Middleware stopped execution\n");
            if (onError) {
                onError("Middleware stopped execution");
            }
            return;
        }
    }

    // 3. Add subscribers from params to EventHandler
    for (auto& subscriber : params.subscribers) {
        _eventHandler->addSubscriber(subscriber);
    }
    printf("[AGenUI][DEBUG] Total subscribers: %zu\n", params.subscribers.size());

    // 4. Build HTTP request
    HttpRequest request;
    request.url = _baseUrl;
    request.method = HttpMethod::POST;
    request.headers = _headers;
    request.body = input.toJson().dump();

    printf("[AGenUI][DEBUG] Sending request to %s\n", _baseUrl.c_str());
    printf("[AGenUI][DEBUG] Request body size: %zu bytes\n", request.body.size());

    // 5. Send request
    _httpService->sendSseRequest(
        request,
        [this, onSuccess, onError](const HttpResponse& response) {
            this->handleResponse(response, onSuccess, onError);
        },
        [this, onSuccess, onError](const HttpResponse& response) {
             this->handleResponse(response, onSuccess, onError);
         },
        [onError](const AgentError& error) {
            onError("sse failed");
        });
}

void HttpAgent::handleResponse(const HttpResponse& response, AgentSuccessCallback onSuccess,
                                AgentErrorCallback onError) {
    // Check if HTTP request succeeded
    if (!response.isSuccess()) {
        printf("[AGenUI][ERROR] HTTP request failed with status: %d\n", response.statusCode);
        if (onError) {
            onError("HTTP request failed with status: " + std::to_string(response.statusCode));
        }
        return;
    }

    printf("[AGenUI][INFO] Received response (%d), parsing SSE events\n", response.statusCode);
    printf("[AGenUI][DEBUG] Response size: %zu bytes\n", response.content.size());

    // Clear parser state and feed data
    _sseParser->clear();
    _sseParser->feed(response.content);

    // Record initial message IDs to identify new messages
    std::set<MessageId> initialMessageIds;
    for (const auto& msg : _eventHandler->messages()) {
        initialMessageIds.insert(msg.id());
    }

    // Prepare middleware context
    MiddlewareContext middlewareContext(nullptr, nullptr);
    middlewareContext.currentMessages = &_eventHandler->messages();
    middlewareContext.currentState = &_eventHandler->state();

    // Process all SSE events
    while (_sseParser->hasEvent()) {
        try {
            const std::string &eventData = _sseParser->nextEvent();
            if (eventData.empty()) {
                continue;
            }
            nlohmann::json eventJson = nlohmann::json::parse(eventData);
            // Parse as Event object
            std::unique_ptr<Event> event(EventParser::parse(eventJson));

            if (!event) {
                continue;
            }
            // Process event through middleware
            if (_middlewareChain.size() > 0) {
                auto processedEvents = _middlewareChain.processEvent(std::move(event), middlewareContext);
                
                // Process all returned events
                for (auto& processedEvent : processedEvents) {
                    AgentStateMutation mutation = _eventHandler->handleEvent(std::move(processedEvent));
                    if (mutation.hasChanges()) {
                        _eventHandler->applyMutation(mutation);
                        // Update middleware context
                        middlewareContext.currentMessages = &_eventHandler->messages();
                        middlewareContext.currentState = &_eventHandler->state();
                    }
                }
            } else {
                // No middleware, process directly
                AgentStateMutation mutation = _eventHandler->handleEvent(std::move(event));
                if (mutation.hasChanges()) {
                    _eventHandler->applyMutation(mutation);
                }
            }
        } catch (const std::exception& e) {
            printf("[AGenUI][ERROR] Error processing %s\n", e.what());
        }
    }

    // Check for parsing errors
    if (!_sseParser->getLastError().empty()) {
        printf("[AGenUI][WARN] SSE parser error: %s\n", _sseParser->getLastError().c_str());
    }

    // Collect results
    RunAgentResult result;
    result.newState = _eventHandler->state();
    result.result = _eventHandler->result();
    result.threadId = "";

    // Identify new messages
    for (const auto& msg : _eventHandler->messages()) {
        if (initialMessageIds.find(msg.id()) == initialMessageIds.end()) {
            result.newMessages.push_back(msg);
        }
    }

    // Process response through middleware
    if (_middlewareChain.size() > 0) {
        printf("[AGenUI][INFO] Processing response through %zu middlewares\n", _middlewareChain.size());
        result = _middlewareChain.processResponse(result, middlewareContext);
    }

    // Call success callback
    if (onSuccess) {
        onSuccess(result);
    }
}

}  // namespace agenui
