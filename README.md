# Multiplayer Sudoku Game

A real-time multiplayer Sudoku game developed in C++ for a Computer Networks project.  
This project uses both **TCP** and **UDP** communication within a client-server architecture to support reliable game coordination and responsive multiplayer interaction.

## Features
- Real-time multiplayer Sudoku gameplay
- Client-server networking model
- Public and private room system
- Login system
- Chatbox
- Leaderboard
- SQLite database integration
- Pencil mode / note-taking
- Hint support

## Networking
This project incorporates both **TCP** and **UDP** concepts:

- **TCP**
  - Reliable client-server communication
  - Login and account-related requests
  - Room creation and room joining
  - Chat messaging
  - Game state synchronization requiring reliable delivery

- **UDP**
  - Lightweight communication for selected real-time multiplayer updates
  - Faster transmission for time-sensitive interactions where low overhead is preferred

The combination of TCP and UDP was used to demonstrate key computer networking concepts such as connection handling, packet flow, reliability, and real-time communication trade-offs.

## Tech Stack
- C++
- TCP / UDP socket programming
- Winsock
- SQLite
- Visual Studio

## Folder Structure
- `GameClient/` — client application, UI, gameplay interaction
- `GameServer/` — server-side logic, matchmaking, validation, and networking
- `Shared/` — shared packet definitions, models, and wire format / serialization code

## How to Run
1. Open the solution in Visual Studio
2. Build the solution
3. Run the server project first
4. Launch one or more client instances
5. Log in, create or join a room, and start playing

## Academic Context
Developed as part of a **Computer Networks** project, with a focus on applying socket programming, multiplayer communication, and protocol design in a game setting.

## Author
Rishika
