import collections
import os
import random
import sys
import time
from functools import lru_cache
from typing import List

WORD_LENGTH = 5
MAX_GUESSES = 6
WORD_LIST_FILENAME = "words.txt"
PRECOMPUTED_FIRST_GUESS = "CRANE"

GREEN = 'G'
YELLOW = 'Y'
GRAY = 'X'

def load_words(filename):
    script_dir = os.path.dirname(__file__)
    filepath = os.path.join(script_dir, filename)

    if not os.path.exists(filepath):
        print(f"Error: Word list file '{filepath}' not found.")
        sys.exit(1)
    words = set()
    try:
        with open(filepath, 'r') as f:
            for line in f:
                word = line.strip().upper()
                if len(word) == WORD_LENGTH and word.isalpha():
                    words.add(word)
    except Exception as e:
        print(f"Error reading word list file '{filepath}': {e}")
        sys.exit(1)

    if not words:
        print(f"Error: No valid {WORD_LENGTH}-letter words found in '{filepath}'.")
        sys.exit(1)

    print(f"Loaded {len(words)} valid words from '{filepath}'.")
    return list(words)

@lru_cache(maxsize=None)
def get_feedback(guess: str, target: str) -> str:
    if len(guess) != WORD_LENGTH or len(target) != WORD_LENGTH:
        raise ValueError("Guess and target must be of WORD_LENGTH")

    feedback = [GRAY] * WORD_LENGTH
    target_counts = collections.Counter(target)
    guess_list = list(guess)

    for i in range(WORD_LENGTH):
        if guess_list[i] == target[i]:
            feedback[i] = GREEN
            target_counts[guess_list[i]] -= 1
            guess_list[i] = '#'

    for i in range(WORD_LENGTH):
        if guess_list[i] == '#':
            continue
        if guess_list[i] in target_counts and target_counts[guess_list[i]] > 0:
            feedback[i] = YELLOW
            target_counts[guess_list[i]] -= 1

    return "".join(feedback)

def filter_words(possible_words: List[str], guess: str, feedback: str) -> List[str]:
    new_possible_words = []
    for word in possible_words:
        if get_feedback(guess, word) == feedback:
            new_possible_words.append(word)
    return new_possible_words

def calculate_guess_score(guess_candidate: str, possible_words: List[str]) -> int:
    if not possible_words:
        return 0

    feedback_groups = collections.defaultdict(int)
    for target_word in possible_words:
        feedback = get_feedback(guess_candidate, target_word)
        feedback_groups[feedback] += 1

    if not feedback_groups:
        return 0

    max_remaining = max(feedback_groups.values())
    return max_remaining

def choose_best_guess(possible_words: List[str], all_words: List[str]) -> str:
    if not possible_words:
        return ""

    if len(possible_words) == 1:
        return possible_words[0]

    best_guess = ""
    min_max_remaining = float('inf')
    candidate_guesses = all_words


    print(f"Evaluating {len(candidate_guesses)} potential guesses against {len(possible_words)} possible targets...")
    start_time = time.time()

    evaluated_count = 0
    for guess_candidate in candidate_guesses:
        max_remaining = calculate_guess_score(guess_candidate, possible_words)

        evaluated_count += 1
        if evaluated_count % 200 == 0:
             elapsed = time.time() - start_time
             print(f"  ... evaluated {evaluated_count}/{len(candidate_guesses)} guesses ({elapsed:.1f}s elapsed)")


        is_better = False
        if max_remaining < min_max_remaining:
            is_better = True
        elif max_remaining == min_max_remaining:
            if guess_candidate in possible_words and best_guess not in possible_words:
                is_better = True

        if is_better:
            min_max_remaining = max_remaining
            best_guess = guess_candidate

    end_time = time.time()
    print(f"Evaluation complete ({end_time - start_time:.2f}s).")

    if not best_guess:
         print("Warning: No best guess found, picking random possible word.")
         return random.choice(possible_words)

    return best_guess

def get_user_feedback(guess: str) -> str:
    while True:
        try:
            feedback_str = input(f"Enter feedback for '{guess}' (e.g., GXYYX): ").strip().upper()
            if len(feedback_str) != WORD_LENGTH:
                print(f"Error: Feedback must be {WORD_LENGTH} characters long.")
                continue
            if not all(c in [GREEN, YELLOW, GRAY] for c in feedback_str):
                print(f"Error: Feedback must only contain G (Green), Y (Yellow), or X (Gray).")
                continue
            return feedback_str
        except EOFError:
            print("\nExiting.")
            sys.exit(0)
        except Exception as e:
            print(f"An unexpected error occurred: {e}")

def play_wordle():
    all_words = load_words(WORD_LIST_FILENAME)
    possible_words = list(all_words)

    print("\nWelcome to Wordle AI Helper!")
    print(f"Using word list: {WORD_LIST_FILENAME}")
    print(f"Goal: Guess the {WORD_LENGTH}-letter word in {MAX_GUESSES} tries.")
    print("Enter feedback using G (Green), Y (Yellow), X (Gray).")
    print("-" * 30)

    for guess_num in range(1, MAX_GUESSES + 1):
        print(f"\n--- Guess {guess_num}/{MAX_GUESSES} ---")
        print(f"Possible words remaining: {len(possible_words)}")
        if len(possible_words) <= 10:
             print(f"  Options: {', '.join(sorted(possible_words))}")


        if guess_num == 1 and PRECOMPUTED_FIRST_GUESS:
            guess = PRECOMPUTED_FIRST_GUESS
            print(f"Using precomputed first guess: {guess}")
        else:
            guess = choose_best_guess(possible_words, all_words)

        if not guess:
            print("Error: Could not determine a guess.")
            break

        print(f"Suggested guess: {guess}")

        if len(possible_words) == 1 and guess == possible_words[0]:
             print(f"\nOnly one possibility left: {guess}. This must be the word!")
             print("Solved!")
             break

        feedback = get_user_feedback(guess)

        if feedback == GREEN * WORD_LENGTH:
            print(f"\nCongratulations! Solved in {guess_num} guesses.")
            break

        possible_words = filter_words(possible_words, guess, feedback)

        if not possible_words:
            print("\nError: No possible words match the feedback provided.")
            print("Please double-check your feedback entries or the word list.")
            break

    else:
        print(f"\nFailed to guess the word within {MAX_GUESSES} tries.")
        if len(possible_words) > 0:
             print(f"Remaining possible words ({len(possible_words)}):")
             if len(possible_words) <= 20:
                 print(f"  {', '.join(sorted(possible_words))}")
             else:
                  print(f"  (Too many to list)")

if __name__ == "__main__":
    play_wordle()
