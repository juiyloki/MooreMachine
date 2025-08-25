# MooreMachine
Moore machine system implementation in C [COMPLETED]

## Note

This project is a personal solution to a university assignment.  
The assignment focused on the **library code**, which is provided here in a **complete, fully functional, and carefully crafted form**.  

- The official tests and starter materials were provided by the professor and are **intellectual property of the university**, so they are **not published** in this repository.  
- My own tests and example usage of the library are still **work in progress**. They have been temporarily set aside to prioritize my other projects and will be added eventually.  

# Moore Machine Library

This project is a C implementation of a shared library (`libma.so`) that simulates **Moore machine** – a class of deterministic finite state machines used in synchronous digital systems.

A Moore machine is represented as a 6-tuple ⟨X, Y, Q, t, y, q⟩, where:

- **X** – set of input signals 
- **Y** – set of output signals  
- **Q** – set of internal states  
- **t: X × Q → Q** – transition function (computes the next state)  
- **y: Q → Y** – output function (computes outputs)  
- **q ∈ Q** – initial state  

This implementation works with binary automata, where inputs, outputs, and states are stored as bit sequences in `uint64_t` arrays (64 bits per element, least significant bit first).

---

## Features

- Dynamically loaded library (`libma.so`)  
- Configurable number of inputs, outputs, and state bits  
- User-defined transition and output functions  
- Connecting outputs of one automaton to inputs of another  
- Synchronous step execution for multiple automata (`ma_step`)  
- Resistant to memory allocation failures (tested with `memory_tests.c`)  
- No artificial limits on automaton size apart from available memory  

---

## Interface

The interface is defined in [`ma.h`](source/ma.h). Key functions include:

- `ma_create_full` – create a custom automaton with user-provided transition and output functions  
- `ma_create_simple` – simplified constructor, outputs mirror state bits  
- `ma_delete` – free all memory used by an automaton  
- `ma_connect` / `ma_disconnect` – connect or disconnect automata  
- `ma_set_input` / `ma_set_state` – set input signals or internal state manually  
- `ma_get_output` – retrieve the current outputs of the automaton  
- `ma_step` – perform one synchronous computation step on a set of automata  

An example of usage can be found in [`ma_example.c`](source/ma_example.c).

---

## Building

The project uses **GNU Make**. To build the library and the example program:

```bash
make

