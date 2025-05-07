import pygame
import random
import sys

pygame.init()

WHITE = (255, 255, 255)
BLACK = (0, 0, 0)
GRAY = (200, 200, 200)
LIGHT_BLUE = (230, 240, 255)
DARK_BLUE = (0, 0, 139)
GREEN = (0, 200, 0)
RED = (255, 0, 0)
HIGHLIGHT = (240, 240, 150)

WIDTH, HEIGHT = 540, 540
GRID_SIZE = 9
CELL_SIZE = WIDTH // GRID_SIZE

screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("quikc maths")
clock = pygame.time.Clock()

number_font = pygame.font.SysFont('Arial', 35)

class SudokuBoard:
    def __init__(self):
        self.board = [[0 for _ in range(GRID_SIZE)] for _ in range(GRID_SIZE)]
        self.original = [[0 for _ in range(GRID_SIZE)] for _ in range(GRID_SIZE)]
        self.selected_cell = None
        self.generate_board()
        self.save_original()
    
    def save_original(self):
        for i in range(GRID_SIZE):
            for j in range(GRID_SIZE):
                self.original[i][j] = self.board[i][j]
    
    def is_original(self, row, col):
        return self.original[row][col] != 0
    
    def is_valid_move(self, row, col, num):
        for i in range(GRID_SIZE):
            if self.board[row][i] == num:
                return False
        
        for i in range(GRID_SIZE):
            if self.board[i][col] == num:
                return False
        
        box_row, box_col = 3 * (row // 3), 3 * (col // 3)
        for i in range(box_row, box_row + 3):
            for j in range(box_col, box_col + 3):
                if self.board[i][j] == num:
                    return False
        
        return True
    
    def solve_board(self, board):
        empty = self.find_empty(board)
        if not empty:
            return True
        
        row, col = empty
        
        for num in range(1, 10):
            if self.is_valid_for_solve(board, row, col, num):
                board[row][col] = num
                
                if self.solve_board(board):
                    return True
                
                board[row][col] = 0
        
        return False
    
    def is_valid_for_solve(self, board, row, col, num):
        for i in range(GRID_SIZE):
            if board[row][i] == num:
                return False
        
        for i in range(GRID_SIZE):
            if board[i][col] == num:
                return False
        
        box_row, box_col = 3 * (row // 3), 3 * (col // 3)
        for i in range(box_row, box_row + 3):
            for j in range(box_col, box_col + 3):
                if board[i][j] == num:
                    return False
        
        return True
    
    def find_empty(self, board):
        for i in range(GRID_SIZE):
            for j in range(GRID_SIZE):
                if board[i][j] == 0:
                    return (i, j)
        return None
    
    def generate_board(self):
        empty_board = [[0 for _ in range(GRID_SIZE)] for _ in range(GRID_SIZE)]
        self.solve_board(empty_board)
        
        for i in range(GRID_SIZE):
            for j in range(GRID_SIZE):
                self.board[i][j] = empty_board[i][j]
        
        positions = [(r, c) for r in range(GRID_SIZE) for c in range(GRID_SIZE)]
        random.shuffle(positions)
        
        cells_to_remove = 45
        
        for row, col in positions[:cells_to_remove]:
            self.board[row][col] = 0
    
    def find_best_empty_cell(self):
        min_poss = 10
        best_cell = None
        for row in range(GRID_SIZE):
            for col in range(GRID_SIZE):
                if self.board[row][col] == 0:
                    count = sum(1 for num in range(1, 10) if self.is_valid_move(row, col, num))
                    if count < min_poss:
                        min_poss = count
                        best_cell = (row, col)
        return best_cell
    
    def solve_ai(self):
        stack = []
        current_cell = self.find_best_empty_cell()
        if not current_cell:
            return
        stack.append((current_cell, 0))
        
        while stack:
            (row, col), num = stack.pop()
            if num > 9:
                self.board[row][col] = 0
                yield (row, col, 0)
                continue
            if self.board[row][col] != 0:
                continue
            found = False
            for n in range(num, 10):
                if self.is_valid_move(row, col, n):
                    self.board[row][col] = n
                    yield (row, col, n)
                    next_cell = self.find_best_empty_cell()
                    if not next_cell:
                        return
                    stack.append(((row, col), n + 1))
                    stack.append((next_cell, 0))
                    found = True
                    break
            if not found:
                self.board[row][col] = 0
                yield (row, col, 0)
    
    def check_win(self):
        for row in self.board:
            if set(row) != set(range(1, 10)):
                return False
        for col in range(9):
            if set(self.board[row][col] for row in range(9)) != set(range(1, 10)):
                return False
        for box_row in range(0, 9, 3):
            for box_col in range(0, 9, 3):
                box = [self.board[r][c] for r in range(box_row, box_row+3) for c in range(box_col, box_col+3)]
                if set(box) != set(range(1, 10)):
                    return False
        return True

class Game:
    def __init__(self):
        self.board = SudokuBoard()
        self.game_over = False
        self.solver = self.board.solve_ai()
    
    def handle_events(self):
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                sys.exit()
    
    def draw(self):
        screen.fill(WHITE)
        for i in range(GRID_SIZE + 1):
            line_width = 3 if i % 3 == 0 else 1
            pygame.draw.line(screen, BLACK, (0, i * CELL_SIZE), (WIDTH, i * CELL_SIZE), line_width)
            pygame.draw.line(screen, BLACK, (i * CELL_SIZE, 0), (i * CELL_SIZE, HEIGHT), line_width)
        
        for i in range(GRID_SIZE):
            for j in range(GRID_SIZE):
                if self.board.board[i][j] != 0:
                    color = DARK_BLUE if self.board.is_original(i, j) else GREEN
                    number = number_font.render(str(self.board.board[i][j]), True, color)
                    number_rect = number.get_rect(center=(j * CELL_SIZE + CELL_SIZE//2, i * CELL_SIZE + CELL_SIZE//2))
                    screen.blit(number, number_rect)
        
        if self.game_over:
            overlay = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)
            overlay.fill((255, 255, 255, 150))
            screen.blit(overlay, (0, 0))
            font = pygame.font.SysFont('Arial', 40, bold=True)
            text = font.render("Solved!", True, DARK_BLUE)
            text_rect = text.get_rect(center=(WIDTH//2, HEIGHT//2))
            screen.blit(text, text_rect)
        
        pygame.display.flip()
    
    def run(self):
        last_step_time = pygame.time.get_ticks()
        while True:
            self.handle_events()
            current_time = pygame.time.get_ticks()
            if not self.game_over and current_time - last_step_time > 50:
                try:
                    next(self.solver)
                    last_step_time = current_time
                    if self.board.check_win():
                        self.game_over = True
                except StopIteration:
                    self.game_over = True
            self.draw()
            clock.tick(30)

if __name__ == "__main__":
    game = Game()
    game.run()