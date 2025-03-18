import pygame
import random
import numpy as np
from collections import deque
import torch
import torch.nn as nn
import torch.optim as optim

# Initialize Pygame
pygame.init()

# Constants
WHITE = (255, 255, 255)
BLACK = (0, 0, 0)
RED = (255, 0, 0)
GREEN = (0, 255, 0)
YELLOW = (255, 255, 0)
BLUE = (0, 0, 255)

WIDTH, HEIGHT = 600, 600
SNAKE_BLOCK = 20
BASE_SPEED = 30

# DQN Hyperparameters
BATCH_SIZE = 128
GAMMA = 0.95
EPSILON_START = 1.0
EPSILON_MIN = 0.01
EPSILON_DECAY = 0.995
TARGET_UPDATE = 10
LEARNING_RATE = 0.0005
MEMORY_SIZE = 50_000

# Neural Network Architecture
class DQN(nn.Module):
    def __init__(self, input_size, hidden_size, output_size):
        super(DQN, self).__init__()
        self.net = nn.Sequential(
            nn.Linear(input_size, hidden_size),
            nn.ReLU(),
            nn.Linear(hidden_size, hidden_size),
            nn.ReLU(),
            nn.Linear(hidden_size, output_size)
        )
        
    def forward(self, x):
        return self.net(x)


class ReplayBuffer:
    def __init__(self, capacity):
        self.buffer = deque(maxlen=capacity)
    
    def push(self, state, action, reward, next_state, done):
        self.buffer.append((state, action, reward, next_state, done))
    
    def sample(self, batch_size):
        return random.sample(self.buffer, batch_size)
    
    def __len__(self):
        return len(self.buffer)

class DQNAgent:
    def __init__(self, input_size, hidden_size, output_size):
        self.policy_net = DQN(input_size, hidden_size, output_size)
        self.target_net = DQN(input_size, hidden_size, output_size)
        self.episode = 0
        self.load_model()
        self.target_net.load_state_dict(self.policy_net.state_dict())
        self.target_net.eval()

    def load_model(self):
        try:
            checkpoint = torch.load("snake_dqn.pth")
            self.policy_net.load_state_dict(checkpoint['policy_net_state_dict'])
            self.epsilon = checkpoint['epsilon']
            self.episode = checkpoint['episode']
            print("Loaded existing model weights, epsilon, and episode")
        except FileNotFoundError:
            print("No existing model found, starting fresh")
            self.epsilon = EPSILON_START
            self.episode = 0

        self.optimizer = optim.Adam(self.policy_net.parameters(), lr=LEARNING_RATE)
        self.memory = ReplayBuffer(MEMORY_SIZE)
        self.loss_fn = nn.MSELoss()

    def select_action(self, state):
        if random.random() < self.epsilon:
            return random.randint(0, 3)
        
        with torch.no_grad():
            state_tensor = torch.FloatTensor(state)
            q_values = self.policy_net(state_tensor)
            return q_values.argmax().item()
        
    def update_epsilon(self):
        self.epsilon = max(EPSILON_MIN, self.epsilon * EPSILON_DECAY)
        
    def update_target_net(self):
        self.target_net.load_state_dict(self.policy_net.state_dict())
        
    def train(self):
        if len(self.memory) < BATCH_SIZE:
            return
        
        transitions = self.memory.sample(BATCH_SIZE)
        batch = list(zip(*transitions))
        
        states = torch.FloatTensor(np.array(batch[0]))
        actions = torch.LongTensor(np.array(batch[1])).unsqueeze(1)
        rewards = torch.FloatTensor(np.array(batch[2]))
        next_states = torch.FloatTensor(np.array(batch[3]))
        dones = torch.BoolTensor(np.array(batch[4]))
        
        current_q = self.policy_net(states).gather(1, actions)
        
        next_q = self.target_net(next_states).max(1)[0].detach()
        target_q = rewards + (1 - dones.float()) * GAMMA * next_q
        
        loss = self.loss_fn(current_q.squeeze(), target_q)
        
        self.optimizer.zero_grad()
        loss.backward()
        self.optimizer.step()
        
        return loss.item()

class SnakeGame:
    def __init__(self):
        self.screen = pygame.display.set_mode((WIDTH, HEIGHT))
        pygame.display.set_caption('dont kys simulator ver.dqn')
        self.clock = pygame.time.Clock()
        self.font = pygame.font.SysFont(None, 35)
        self.best_score = self.load_best_score()
        
    def load_best_score(self):
        try:
            with open('best_score.txt', 'r') as f:
                return int(f.read())
        except FileNotFoundError:
            return 0
            
    def save_best_score(self, score):
        if score > self.best_score:
            self.best_score = score
            with open('best_score.txt', 'w') as f:
                f.write(str(score))
                
    def draw_score(self, score):
        text = self.font.render(f"Score: {score}   Best: {self.best_score}", True, WHITE)
        self.screen.blit(text, [0, 0])
        
    def generate_food(self, snake):
        while True:
            food = (random.randrange(0, WIDTH, SNAKE_BLOCK),
                    random.randrange(0, HEIGHT, SNAKE_BLOCK))
            if food not in snake:
                return food
            
    def get_state(self, snake, food, direction):
        head = snake[-1]
        
        state = [
            # Food relative position
            (food[0] - head[0]) / WIDTH,
            (food[1] - head[1]) / HEIGHT,
            
            # Direction
            direction[0], 
            direction[1],
            
            # Wall proximity
            head[0] / WIDTH,
            (WIDTH - head[0]) / WIDTH,
            head[1] / HEIGHT,
            (HEIGHT - head[1]) / HEIGHT
        ]
        
        # Body proximity (8 directions)
        for dx, dy in [(-1,0), (1,0), (0,-1), (0,1), 
                      (-1,-1), (-1,1), (1,-1), (1,1)]:
            check = (head[0] + dx*SNAKE_BLOCK, head[1] + dy*SNAKE_BLOCK)
            state.append(1 if check in snake[:-1] else 0)
            
        return np.array(state, dtype=np.float32)
    
    def run(self):
        agent = DQNAgent(input_size=16, hidden_size=256, output_size=4)
        episode = 0
        
        while True:
            episode += 1
            snake = [(WIDTH/2, HEIGHT/2)]
            direction = (0, 0)
            food = self.generate_food(snake)
            score = 0
            done = False
            steps = 0
            
            while not done:
                for event in pygame.event.get():
                    if event.type == pygame.QUIT:
                        pygame.quit()
                        return
                        
                state = self.get_state(snake, food, direction)
                action = agent.select_action(state)
                
                # Map action to direction
                new_dir = [(0, -1), (0, 1), (-1, 0), (1, 0)][action]
                if (new_dir[0] + direction[0], new_dir[1] + direction[1]) != (0, 0):
                    direction = new_dir
                
                # Move snake
                head = (snake[-1][0] + direction[0]*SNAKE_BLOCK,
                        snake[-1][1] + direction[1]*SNAKE_BLOCK)
                
                # Check collisions
                if (head in snake or 
                    head[0] < 0 or head[0] >= WIDTH or 
                    head[1] < 0 or head[1] >= HEIGHT):
                    done = True
                    reward = -10
                else:
                    snake.append(head)
                    if head == food:
                        food = self.generate_food(snake)
                        score += 10
                        reward = 10
                        self.save_best_score(score)
                    else:
                        snake.pop(0)
                        reward = 0
                        
                    # Distance reward
                    prev_dist = abs(snake[-2][0]-food[0]) + abs(snake[-2][1]-food[1]) if len(snake) > 1 else 0
                    new_dist = abs(head[0]-food[0]) + abs(head[1]-food[1])
                    reward += (prev_dist - new_dist) * 0.1
                    
                next_state = self.get_state(snake, food, direction)
                agent.memory.push(state, action, reward, next_state, done)
                
                # Training
                loss = agent.train()
                
                # Render
                self.screen.fill(BLACK)
                for segment in snake:
                    pygame.draw.rect(self.screen, WHITE, 
                                   [segment[0], segment[1], SNAKE_BLOCK, SNAKE_BLOCK])
                pygame.draw.rect(self.screen, GREEN, 
                               [food[0], food[1], SNAKE_BLOCK, SNAKE_BLOCK])
                self.draw_score(score)
                pygame.display.update()
                self.clock.tick(BASE_SPEED)
                
                steps += 1
                if steps > 200 + score * 50:  # Prevent infinite games
                    done = True
                    
            # Update target network
            if episode % TARGET_UPDATE == 0:
                agent.update_target_net()
                
            # Epsilon decay
            agent.update_epsilon()

            # Save model
            torch.save({
                'episode': episode,
                'policy_net_state_dict': agent.policy_net.state_dict(),
                'epsilon': agent.epsilon
            }, "snake_dqn.pth")
            print(f"Episode {episode} | Score: {score} | Epsilon: {agent.epsilon:.2f}")

if __name__ == "__main__":
    game = SnakeGame()
    game.run()
