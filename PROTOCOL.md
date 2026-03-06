# COMMUNICATION PROTOCOL

The Cortex CLI agents communicate via a structured JSON-based protocol over ZeroMQ.

## Message Format

```json
{
  "header": {
    "msg_id": "uuid-v4",
    "sender": "agent_name",
    "timestamp": "iso8601",
    "type": "request | response | signal"
  },
  "payload": {
    "action": "action_name",
    "params": {},
    "status": "success | error | pending",
    "content": "..."
  }
}
```

## ZeroMQ Topology

- **Agent-to-Agent**: PUB/SUB or ROUTER/DEALER for direct coordination.
- **System-Wide**: A central message bus (ROUTER) managing agent registration and task routing.
