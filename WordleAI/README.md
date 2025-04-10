# WordleAI

## Overview

WordleAI implements a basic Minimax algorithm enhanced with information gain optimization for solving Wordle puzzles. The implementation focuses on efficient word selection through strategic decision-making.

As succinctly explained by ChatGPT:
> Treat Wordle as a single-player game with a search tree, where each node is a game state (remaining words, feedback). Use a minimax-like approach to pick the guess that minimizes the worst-case number of remaining words (or maximizes information gain).

The word list utilized in this implementation is sourced from this [GitHub repository](https://github.com/Kinkelin/WordleCompetition/blob/main/data/official/wordle_historic_words.txt), providing a comprehensive and reliable dataset for the algorithm.
