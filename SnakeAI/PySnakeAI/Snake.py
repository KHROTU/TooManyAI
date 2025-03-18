import pygame
import random
import heapq
import pygame.font
from collections import deque

pygame.init()

WHITE = (255, 255, 255)
BLACK = (0, 0, 0)
RED = (255, 0, 0)
GREEN = (0, 255, 0)
YELLOW = (255, 255, 0)
BLUE = (0, 0, 255)

WIDTH = 600
HEIGHT = 600
dis = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption('dont kys simulator')

SNAKE_BLOCK = 20
BASE_SPEED = 120
SAFE_SPEED = 32
current_speed = BASE_SPEED
survival_mode = False

clock = pygame.time.Clock()

font_style = pygame.font.SysFont(None, 50)
score_font = pygame.font.SysFont(None, 35)

try:
    with open('best_score.txt', 'r') as f:
        best_score = int(f.read())
except FileNotFoundError:
    best_score = 0

def Your_score(score):
    global best_score, survival_mode
    if score > best_score:
        best_score = score
        with open('best_score.txt', 'w') as f:
            f.write(str(best_score))
    color = RED if survival_mode else WHITE
    value = score_font.render(f"Score: {score}   Best: {best_score}", True, color)
    dis.blit(value, [0, 0])

def message(msg):
    mesg = font_style.render(msg, True, RED)
    dis.blit(mesg, [WIDTH/6, HEIGHT/3])

def food_position(snake_list):
    while True:
        food_x = round(random.randrange(0, WIDTH - SNAKE_BLOCK) / SNAKE_BLOCK) * SNAKE_BLOCK
        food_y = round(random.randrange(0, HEIGHT - SNAKE_BLOCK) / SNAKE_BLOCK) * SNAKE_BLOCK
        if (food_x, food_y) not in [(segment[0], segment[1]) for segment in snake_list]:
            break
    return food_x, food_y

def heuristic(a, b):
    return abs(a[0] - b[0]) + abs(a[1] - b[1])

def get_neighbors(node):
    x, y = node
    return [(x+SNAKE_BLOCK, y), (x-SNAKE_BLOCK, y), (x, y+SNAKE_BLOCK), (x, y-SNAKE_BLOCK)]

def get_grid_neighbors(node):
    x, y = node
    return [(x+1, y), (x-1, y), (x, y+1), (x, y-1)]

def reconstruct_path(came_from, current):
    path = []
    while current in came_from:
        path.append(current)
        current = came_from[current]
    path.append(current)
    path.reverse()
    return path

def a_star(start, goal, obstacles, grid_width, grid_height):
    open_list = []
    heapq.heappush(open_list, (0, start))
    came_from = {}
    g_score = {start: 0}
    f_score = {start: heuristic(start, goal)}
    open_set = {start}

    while open_list:
        _, current = heapq.heappop(open_list)
        open_set.remove(current)

        if current == goal:
            return reconstruct_path(came_from, current)

        for neighbor in get_grid_neighbors(current):
            if neighbor[0] < 0 or neighbor[0] >= grid_width or neighbor[1] < 0 or neighbor[1] >= grid_height:
                continue
            if neighbor in obstacles:
                continue
                
            tentative_g = g_score[current] + 1
            if neighbor not in g_score or tentative_g < g_score[neighbor]:
                came_from[neighbor] = current
                g_score[neighbor] = tentative_g
                f = tentative_g + heuristic(neighbor, goal)
                
                if neighbor not in open_set:
                    heapq.heappush(open_list, (f, neighbor))
                    open_set.add(neighbor)
    return None

def flood_fill(start, obstacles, grid_width, grid_height):
    visited = set()
    queue = deque([start])
    visited.add(start)
    
    while queue:
        node = queue.popleft()
        
        for neighbor in get_grid_neighbors(node):
            if (0 <= neighbor[0] < grid_width and 
                0 <= neighbor[1] < grid_height and 
                neighbor not in visited and 
                neighbor not in obstacles):
                visited.add(neighbor)
                queue.append(neighbor)
                
    return len(visited)

def evaluate_move(head, move, obstacles, grid_width, grid_height, food_pos, survival_mode):
    dx, dy = move
    new_head = (head[0] + dx, head[1] + dy)

    if (new_head[0] < 0 or new_head[0] >= grid_width or
        new_head[1] < 0 or new_head[1] >= grid_height or
        new_head in obstacles):
        return -float('inf'), 0, 0, 0

    new_obstacles = obstacles | {new_head}
    space = flood_fill(new_head, new_obstacles, grid_width, grid_height)

    free_neighbors = 0
    for neighbor in get_grid_neighbors(new_head):
        if (0 <= neighbor[0] < grid_width and
            0 <= neighbor[1] < grid_height and
            neighbor not in new_obstacles):
            free_neighbors += 1

    food_dist = heuristic(new_head, food_pos)
    
    if survival_mode:
        # Prioritize maximizing free space
        score = space * 1000 + free_neighbors * 200
    else:
        # Prioritize minimizing distance to food
        score = space * 1000 - food_dist

    return score, space, free_neighbors, food_dist

def find_safest_move(head, obstacles, grid_width, grid_height, food_pos, survival_mode):
    best_move = None
    best_score = -float('inf')
    max_space = 0
    max_neighbors = 0
    min_food_dist = float('inf')

    moves = [(1, 0), (-1, 0), (0, 1), (0, -1)]
    random.shuffle(moves)

    for move in moves:
        score, space, neighbors, food_dist = evaluate_move(head, move, obstacles, grid_width, grid_height, food_pos, survival_mode)
        if survival_mode:
            if score > best_score or (score == best_score and neighbors > max_neighbors):
                best_score = score
                best_move = move
                max_space = space
                max_neighbors = neighbors
        else:
            if score > best_score or (score == best_score and food_dist < min_food_dist):
                best_score = score
                best_move = move
                min_food_dist = food_dist

    if survival_mode and max_space < (grid_width * grid_height) // 10:
        max_neighbors = 0
        best_move = None
        for move in moves:
            _, space, neighbors, _ = evaluate_move(head, move, obstacles, grid_width, grid_height, food_pos, survival_mode)
            if neighbors > max_neighbors or (neighbors == max_neighbors and space > max_space):
                max_neighbors = neighbors
                best_move = move
                max_space = space

    return best_move

def gameLoop():
    global best_score, current_speed, survival_mode
    game_over = False
    game_close = False

    x1 = WIDTH / 2
    y1 = HEIGHT / 2

    x1_change = 0
    y1_change = 0

    snake_length = 1
    snake_List = []
    food_x, food_y = food_position(snake_List)
    score = 0
    
    last_path = None
    path_timeout = 0

    while not game_over:
        while game_close:
            dis.fill(BLACK)
            message("Game Over! Press Q or C")
            Your_score(score)
            pygame.display.update()

            for event in pygame.event.get():
                if event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_q:
                        game_over = True
                        game_close = False
                    if event.key == pygame.K_c:
                        current_speed = BASE_SPEED
                        survival_mode = False
                        gameLoop()

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                game_over = True

        grid_head = (int(x1 // SNAKE_BLOCK), int(y1 // SNAKE_BLOCK))
        grid_food = (int(food_x // SNAKE_BLOCK), int(food_y // SNAKE_BLOCK))
        grid_width = WIDTH // SNAKE_BLOCK
        grid_height = HEIGHT // SNAKE_BLOCK

        obstacles = set()
        for segment in snake_List[:-1]:
            grid_segment = (int(segment[0] // SNAKE_BLOCK), int(segment[1] // SNAKE_BLOCK))
            obstacles.add(grid_segment)

        if len(snake_List) > 0 and len(snake_List) > snake_length - 1:
            tail_pos = snake_List[0]
            grid_tail = (int(tail_pos[0] // SNAKE_BLOCK), int(tail_pos[1] // SNAKE_BLOCK))
            if grid_tail in obstacles:
                obstacles.remove(grid_tail)
        
        accessible_space = flood_fill(grid_head, obstacles, grid_width, grid_height)
        total_possible_space = grid_width * grid_height - len(obstacles)
        has_inaccessible_areas = accessible_space < total_possible_space
        path_to_food = a_star(grid_head, grid_food, obstacles, grid_width, grid_height)
        can_reach_food = path_to_food is not None
        
        survival_mode = has_inaccessible_areas and not can_reach_food
        current_speed = SAFE_SPEED if survival_mode else BASE_SPEED
        
        path = None
        if not survival_mode:
            if path_timeout > 0:
                path_timeout -= 1
                
            if last_path is None or not last_path or path_timeout <= 0:
                path = a_star(grid_head, grid_food, obstacles, grid_width, grid_height)
                if path:
                    last_path = path
                    path_timeout = 3
            else:
                path = last_path
                if len(path) > 1 and path[0] != grid_head:
                    try:
                        head_idx = path.index(grid_head)
                        path = path[head_idx:]
                    except ValueError:
                        path = a_star(grid_head, grid_food, obstacles, grid_width, grid_height)
                        last_path = path
        else:
            path = None

        if path and len(path) >= 2 and not survival_mode:
            next_node = path[1]
            dx = next_node[0] - grid_head[0]
            dy = next_node[1] - grid_head[1]
        else:
            safest_move = find_safest_move(grid_head, obstacles, grid_width, grid_height, grid_food, survival_mode)
            if safest_move:
                dx, dy = safest_move
            else:
                possible_moves = []
                for move_dx, move_dy in [(1, 0), (-1, 0), (0, 1), (0, -1)]:
                    new_x = grid_head[0] + move_dx
                    new_y = grid_head[1] + move_dy
                    if (0 <= new_x < grid_width and 0 <= new_y < grid_height and 
                        (new_x, new_y) not in obstacles):
                        possible_moves.append((move_dx, move_dy))
                
                if possible_moves:
                    dx, dy = random.choice(possible_moves)
                else:
                    dx, dy = 0, 0

        x1_change = dx * SNAKE_BLOCK
        y1_change = dy * SNAKE_BLOCK

        if x1 >= WIDTH or x1 < 0 or y1 >= HEIGHT or y1 < 0:
            game_close = True

        x1 += x1_change
        y1 += y1_change
        dis.fill(BLACK)
        pygame.draw.rect(dis, GREEN, [food_x, food_y, SNAKE_BLOCK, SNAKE_BLOCK])
        
        snake_Head = [x1, y1]
        snake_List.append(snake_Head)
        if len(snake_List) > snake_length:
            del snake_List[0]

        for x in snake_List[:-1]:
            if x == snake_Head:
                game_close = True

        snake_color = RED if survival_mode else WHITE
        for segment in snake_List:
            pygame.draw.rect(dis, snake_color, [segment[0], segment[1], SNAKE_BLOCK, SNAKE_BLOCK])

        if x1 == food_x and y1 == food_y:
            food_x, food_y = food_position(snake_List)
            snake_length += 1
            score += 10
            last_path = None

        Your_score(score)
        pygame.display.update()
        clock.tick(current_speed)

    pygame.quit()
    quit()

gameLoop()
