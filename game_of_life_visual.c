#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <omp.h>

// Define ANSI color codes for terminal output
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_BOLD          "\x1b[1m"

#define GRID_SIZE 100
#define CELL_SIZE 8
#define WINDOW_WIDTH (GRID_SIZE * CELL_SIZE)
#define WINDOW_HEIGHT (GRID_SIZE * CELL_SIZE)
#define CENTER_SIZE 10
#define DELAY_MS 50
#define ITERATIONS 100  // Exactly 100 generations as required

// Function prototypes
void initialize_grid(char grid[GRID_SIZE][GRID_SIZE]);
void initialize_random_grid(char grid[GRID_SIZE][GRID_SIZE], float density);
void initialize_glider_grid(char grid[GRID_SIZE][GRID_SIZE]);
int count_neighbors(char grid[GRID_SIZE][GRID_SIZE], int row, int col);
void update_grid_serial(char grid[GRID_SIZE][GRID_SIZE], char next_grid[GRID_SIZE][GRID_SIZE]);
void update_grid_parallel(char grid[GRID_SIZE][GRID_SIZE], char next_grid[GRID_SIZE][GRID_SIZE]);
void render_grid(SDL_Renderer *renderer, char grid[GRID_SIZE][GRID_SIZE], int live_count);
int count_live_cells(char grid[GRID_SIZE][GRID_SIZE]);
void print_simulation_info(int generation, int live_count, double elapsed_time, bool is_parallel);
void print_help_menu();
void draw_stats_overlay(SDL_Renderer *renderer, int generation, int live_count, double elapsed_time, bool is_parallel);

int main(int argc, char* argv[]) {
    // Parse command line arguments
    bool use_parallel = false;
    int pattern_choice = 0; // 0: standard, 1: random, 2: glider
    float random_density = 0.3f;
    bool show_help = false;
    bool show_stats = true;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--parallel") == 0) {
            use_parallel = true;
        } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--random") == 0) {
            pattern_choice = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                random_density = atof(argv[i + 1]);
                if (random_density <= 0 || random_density >= 1) {
                    random_density = 0.3f;
                }
                i++;
            }
        } else if (strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "--glider") == 0) {
            pattern_choice = 2;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help = true;
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--no-stats") == 0) {
            show_stats = false;
        }
    }
    
    if (show_help) {
        print_help_menu();
        return 0;
    }
    
    // Print program banner
    printf("\n");
    printf(ANSI_COLOR_CYAN ANSI_BOLD "╔════════════════════════════════════════════════════════════╗\n" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_CYAN ANSI_BOLD "║                  CONWAY'S GAME OF LIFE                     ║\n" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_CYAN ANSI_BOLD "╚════════════════════════════════════════════════════════════╝\n" ANSI_COLOR_RESET);
    printf("\n");
    
    // Print execution mode
    printf(ANSI_COLOR_YELLOW "Execution mode: %s\n" ANSI_COLOR_RESET, use_parallel ? "Parallel" : "Serial");
    printf(ANSI_COLOR_YELLOW "Initial pattern: %s\n" ANSI_COLOR_RESET, 
           pattern_choice == 0 ? "Standard (Center Square)" : 
           pattern_choice == 1 ? "Random" : "Glider");
    if (pattern_choice == 1) {
        printf(ANSI_COLOR_YELLOW "Random density: %.2f\n" ANSI_COLOR_RESET, random_density);
    }
    printf("\n");
    
    // Controls information
    printf(ANSI_COLOR_GREEN "Controls:\n" ANSI_COLOR_RESET);
    printf("  • ESC: Exit simulation\n");
    printf("  • P: Toggle parallel/serial processing\n");
    printf("  • SPACE: Pause for 3 seconds\n");
    printf("  • R: Reset grid with random pattern\n");
    printf("  • S: Toggle statistics overlay\n");
    printf("\n");
    
    char grid[GRID_SIZE][GRID_SIZE];
    char next_grid[GRID_SIZE][GRID_SIZE];
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, ANSI_COLOR_RED "SDL could not initialize! SDL_Error: %s\n" ANSI_COLOR_RESET, SDL_GetError());
        return 1;
    }
    
    // Create window
    SDL_Window *window = SDL_CreateWindow("Conway's Game of Life", 
                                          SDL_WINDOWPOS_UNDEFINED, 
                                          SDL_WINDOWPOS_UNDEFINED, 
                                          WINDOW_WIDTH, WINDOW_HEIGHT, 
                                          SDL_WINDOW_SHOWN);
    if (window == NULL) {
        fprintf(stderr, ANSI_COLOR_RED "Window could not be created! SDL_Error: %s\n" ANSI_COLOR_RESET, SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    // Create renderer
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        fprintf(stderr, ANSI_COLOR_RED "Renderer could not be created! SDL_Error: %s\n" ANSI_COLOR_RESET, SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Initialize grid based on pattern choice
    switch (pattern_choice) {
        case 1:
            initialize_random_grid(grid, random_density);
            break;
        case 2:
            initialize_glider_grid(grid);
            break;
        default:
            initialize_grid(grid);
            break;
    }
    
    // Main loop
    bool quit = false;
    SDL_Event e;
    int generation = 1; // Start from 1 as requested
    
    // For timing
    double start_time = omp_get_wtime();
    double generation_start_time = start_time;
    double elapsed_time = 0.0;
    
    // Statistics tracking
    int max_live_cells = 0;
    int min_live_cells = GRID_SIZE * GRID_SIZE;
    double total_time_serial = 0.0;
    double total_time_parallel = 0.0;
    int serial_generations = 0;
    int parallel_generations = 0;
    
    while (!quit && generation <= ITERATIONS) { // Run for exactly 100 generations (1-100)
        generation_start_time = omp_get_wtime();
        
        // Handle events
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    quit = true;
                }
                else if (e.key.keysym.sym == SDLK_p) {
                    // Toggle parallel/serial processing
                    use_parallel = !use_parallel;
                    printf(ANSI_COLOR_YELLOW "Switched to %s processing\n" ANSI_COLOR_RESET, use_parallel ? "parallel" : "serial");
                }
                else if (e.key.keysym.sym == SDLK_SPACE) {
                    // Pause/resume
                    printf(ANSI_COLOR_BLUE "Simulation paused for 3 seconds\n" ANSI_COLOR_RESET);
                    SDL_Delay(3000); // Pause for 3 seconds
                }
                else if (e.key.keysym.sym == SDLK_r) {
                    // Reset with random pattern
                    initialize_random_grid(grid, random_density);
                    printf(ANSI_COLOR_GREEN "Reset grid with random pattern (density: %.2f)\n" ANSI_COLOR_RESET, random_density);
                }
                else if (e.key.keysym.sym == SDLK_s) {
                    // Toggle statistics overlay
                    show_stats = !show_stats;
                }
            }
        }
        
        // Clear screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        // Count live cells
        int live_count = count_live_cells(grid);
        
        // Update statistics
        if (live_count > max_live_cells) max_live_cells = live_count;
        if (live_count < min_live_cells) min_live_cells = live_count;
        
        // Render grid
        render_grid(renderer, grid, live_count);
        
        // Draw statistics overlay if enabled
        if (show_stats) {
            draw_stats_overlay(renderer, generation, live_count, elapsed_time, use_parallel);
        }
        
        // Update screen
        SDL_RenderPresent(renderer);
        
        // Update grid for next generation
        if (use_parallel) {
            update_grid_parallel(grid, next_grid);
            parallel_generations++;
        } else {
            update_grid_serial(grid, next_grid);
            serial_generations++;
        }
        
        // Calculate elapsed time for this generation
        double generation_end_time = omp_get_wtime();
        elapsed_time = generation_end_time - generation_start_time;
        
        // Update timing statistics
        if (use_parallel) {
            total_time_parallel += elapsed_time;
        } else {
            total_time_serial += elapsed_time;
        }
        
        // Display generation counter and live cell count
        char title[100];
        sprintf(title, "Conway's Game of Life - Gen: %d/%d - Live Cells: %d - %s", 
                generation, ITERATIONS, live_count, use_parallel ? "Parallel" : "Serial");
        SDL_SetWindowTitle(window, title);
        
        // Print generation information to terminal
        print_simulation_info(generation, live_count, elapsed_time, use_parallel);
        
        // Delay to make the simulation visible
        SDL_Delay(DELAY_MS);
        
        generation++;
    }
    
    // Calculate and display total execution time
    double end_time = omp_get_wtime();
    double total_time = end_time - start_time;
    
    // Print final statistics
    printf("\n");
    printf(ANSI_COLOR_CYAN ANSI_BOLD "╔════════════════════════════════════════════════════════════╗\n" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_CYAN ANSI_BOLD "║                  SIMULATION COMPLETED                      ║\n" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_CYAN ANSI_BOLD "╚════════════════════════════════════════════════════════════╝\n" ANSI_COLOR_RESET);
    printf("\n");
    
    printf(ANSI_COLOR_YELLOW "Total execution time: %.4f seconds\n" ANSI_COLOR_RESET, total_time);
    printf(ANSI_COLOR_YELLOW "Average time per generation: %.4f seconds\n" ANSI_COLOR_RESET, total_time / ITERATIONS);
    printf("\n");
    
    printf(ANSI_COLOR_GREEN "Performance Statistics:\n" ANSI_COLOR_RESET);
    printf("  • Maximum live cells: %d\n", max_live_cells);
    printf("  • Minimum live cells: %d\n", min_live_cells);
    printf("  • Serial generations: %d\n", serial_generations);
    printf("  • Parallel generations: %d\n", parallel_generations);
    
    if (serial_generations > 0) {
        printf("  • Average serial generation time: %.6f seconds\n", total_time_serial / serial_generations);
    }
    if (parallel_generations > 0) {
        printf("  • Average parallel generation time: %.6f seconds\n", total_time_parallel / parallel_generations);
    }
    if (serial_generations > 0 && parallel_generations > 0) {
        double avg_serial = total_time_serial / serial_generations;
        double avg_parallel = total_time_parallel / parallel_generations;
        double speedup = avg_serial / avg_parallel;
        printf("  • Parallel speedup: %.2fx\n", speedup);
    }
    
    // Final pause to see the end result
    printf("\n");
    printf(ANSI_COLOR_BLUE "Final state displayed for 3 seconds before exiting...\n" ANSI_COLOR_RESET);
    SDL_Delay(3000);
    
    // Clean up
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}

// Print help menu
void print_help_menu() {
    printf(ANSI_COLOR_CYAN ANSI_BOLD "╔════════════════════════════════════════════════════════════╗\n" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_CYAN ANSI_BOLD "║                  CONWAY'S GAME OF LIFE                     ║\n" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_CYAN ANSI_BOLD "╚════════════════════════════════════════════════════════════╝\n" ANSI_COLOR_RESET);
    printf("\n");
    printf(ANSI_COLOR_YELLOW "Usage: gameoflife [OPTIONS]\n" ANSI_COLOR_RESET);
    printf("\n");
    printf(ANSI_COLOR_GREEN "Options:\n" ANSI_COLOR_RESET);
    printf("  -p, --parallel       Enable parallel processing\n");
    printf("  -r, --random [DENS]  Initialize with random pattern (optional density 0.0-1.0)\n");
    printf("  -g, --glider         Initialize with glider pattern\n");
    printf("  -n, --no-stats       Disable statistics overlay\n");
    printf("  -h, --help           Display this help message\n");
    printf("\n");
    printf(ANSI_COLOR_GREEN "Controls:\n" ANSI_COLOR_RESET);
    printf("  ESC                  Exit simulation\n");
    printf("  P                    Toggle parallel/serial processing\n");
    printf("  SPACE                Pause for 3 seconds\n");
    printf("  R                    Reset grid with random pattern\n");
    printf("  S                    Toggle statistics overlay\n");
}

// Print simulation information to terminal
void print_simulation_info(int generation, int live_count, double elapsed_time, bool is_parallel) {
    char progress_bar[51] = {0};
    int progress = (generation * 50) / ITERATIONS;
    
    for (int i = 0; i < 50; i++) {
        if (i < progress) {
            progress_bar[i] = '=';
        } else if (i == progress) {
            progress_bar[i] = '>';
        } else {
            progress_bar[i] = ' ';
        }
    }
    
    printf(ANSI_COLOR_CYAN "[%s] %3d%%" ANSI_COLOR_RESET " | ", progress_bar, generation * 100 / ITERATIONS);
    printf(ANSI_COLOR_YELLOW "Gen: %3d/%3d" ANSI_COLOR_RESET " | ", generation, ITERATIONS);
    printf(ANSI_COLOR_GREEN "Live Cells: %5d" ANSI_COLOR_RESET " | ", live_count);
    printf(ANSI_COLOR_MAGENTA "Time: %.6f s" ANSI_COLOR_RESET " | ", elapsed_time);
    printf(ANSI_COLOR_BLUE "Mode: %s" ANSI_COLOR_RESET, is_parallel ? "Parallel" : "Serial");
    printf("\r");
    fflush(stdout);
    
    // Print newline at the end of simulation
    if (generation == ITERATIONS) {
        printf("\n");
    }
}

// Draw statistics overlay on the SDL window
void draw_stats_overlay(SDL_Renderer *renderer, int generation, int live_count, double elapsed_time, bool is_parallel) {
    // Background for statistics
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_Rect stats_bg = {10, 10, 300, 100};
    SDL_RenderFillRect(renderer, &stats_bg);
    
    // Border for statistics
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &stats_bg);
    
    // Create text as colored rectangles (since SDL doesn't have text rendering by default)
    // This is a simple visualization of text using colored rectangles
    
    // Generation indicator
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
    int progress = (generation * 280) / ITERATIONS;
    SDL_Rect progress_bar = {20, 30, 280, 10};
    SDL_RenderDrawRect(renderer, &progress_bar);
    SDL_Rect progress_fill = {20, 30, progress, 10};
    SDL_RenderFillRect(renderer, &progress_fill);
    
    // Live cells indicator
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    int cell_bar = (live_count * 280) / (GRID_SIZE * GRID_SIZE / 4); // Assuming max is 25% of grid
    if (cell_bar > 280) cell_bar = 280;
    SDL_Rect cell_bar_bg = {20, 50, 280, 10};
    SDL_RenderDrawRect(renderer, &cell_bar_bg);
    SDL_Rect cell_bar_fill = {20, 50, cell_bar, 10};
    SDL_RenderFillRect(renderer, &cell_bar_fill);
    
    // Performance indicator
    SDL_SetRenderDrawColor(renderer, is_parallel ? 100, 100, 255, 255 : 255, 100, 100, 255);
    int speed_bar = (1.0 / (elapsed_time + 0.001)) * 10; // Inverse of time for speed
    if (speed_bar > 280) speed_bar = 280;
    SDL_Rect speed_bar_bg = {20, 70, 280, 10};
    SDL_RenderDrawRect(renderer, &speed_bar_bg);
    SDL_Rect speed_bar_fill = {20, 70, speed_bar, 10};
    SDL_RenderFillRect(renderer, &speed_bar_fill);
    
    // Mode indicator
    SDL_SetRenderDrawColor(renderer, is_parallel ? 100, 100, 255, 255 : 255, 100, 100, 255);
    SDL_Rect mode_indicator = {20, 90, 20, 10};
    SDL_RenderFillRect(renderer, &mode_indicator);
}

// Initialize the grid with the center 10x10 area as live cells
void initialize_grid(char grid[GRID_SIZE][GRID_SIZE]) {
    // Set all cells to dead
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            grid[i][j] = '.';
        }
    }
    
    // Set center 10x10 area to live cells
    int start_row = (GRID_SIZE - CENTER_SIZE) / 2;
    int start_col = (GRID_SIZE - CENTER_SIZE) / 2;
    
    for (int i = 0; i < CENTER_SIZE; i++) {
        for (int j = 0; j < CENTER_SIZE; j++) {
            grid[start_row + i][start_col + j] = '*';
        }
    }
}

// Initialize the grid with random live cells
void initialize_random_grid(char grid[GRID_SIZE][GRID_SIZE], float density) {
    srand(time(NULL));
    
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            if ((float)rand() / RAND_MAX < density) {
                grid[i][j] = '*';
            } else {
                grid[i][j] = '.';
            }
        }
    }
}

// Initialize the grid with a glider pattern
void initialize_glider_grid(char grid[GRID_SIZE][GRID_SIZE]) {
    // Clear the grid
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            grid[i][j] = '.';
        }
    }
    
    // Add glider in the top-left corner
    int start_row = 10;
    int start_col = 10;
    
    // Glider pattern
    grid[start_row][start_col+1] = '*';
    grid[start_row+1][start_col+2] = '*';
    grid[start_row+2][start_col] = '*';
    grid[start_row+2][start_col+1] = '*';
    grid[start_row+2][start_col+2] = '*';
    
    // Add some random cells
    for (int i = 0; i < 100; i++) {
        int row = rand() % GRID_SIZE;
        int col = rand() % GRID_SIZE;
        grid[row][col] = '*';
    }
}

// Count the number of live neighbors for a given cell (with toroidal boundary)
int count_neighbors(char grid[GRID_SIZE][GRID_SIZE], int row, int col) {
    int count = 0;
    
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            // Skip the cell itself
            if (i == 0 && j == 0) continue;
            
            // Calculate neighbor position with toroidal boundary
            int neighbor_row = (row + i + GRID_SIZE) % GRID_SIZE;
            int neighbor_col = (col + j + GRID_SIZE) % GRID_SIZE;
            
            if (grid[neighbor_row][neighbor_col] == '*') {
                count++;
            }
        }
    }
    
    return count;
}

// Update the grid for the next generation - serial version
void update_grid_serial(char grid[GRID_SIZE][GRID_SIZE], char next_grid[GRID_SIZE][GRID_SIZE]) {
    // Calculate next generation
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            int neighbors = count_neighbors(grid, i, j);
            
            // Apply the rules of the Game of Life
            if (grid[i][j] == '*') {
                // Cell is alive
                if (neighbors < 2 || neighbors > 3) {
                    // Dies from underpopulation or overpopulation
                    next_grid[i][j] = '.';
                } else {
                    // Survives
                    next_grid[i][j] = '*';
                }
            } else {
                // Cell is dead
                if (neighbors == 3) {
                    // Reproduction
                    next_grid[i][j] = '*';
                } else {
                    // Stays dead
                    next_grid[i][j] = '.';
                }
            }
        }
    }
    
    // Copy next_grid back to grid for the next iteration
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            grid[i][j] = next_grid[i][j];
        }
    }
}

// Update the grid for the next generation - parallel version with guided scheduling
void update_grid_parallel(char grid[GRID_SIZE][GRID_SIZE], char next_grid[GRID_SIZE][GRID_SIZE]) {
    // Calculate next generation in parallel
    #pragma omp parallel for schedule(guided, 1)
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            int neighbors = count_neighbors(grid, i, j);
            
            // Apply the rules of the Game of Life
            if (grid[i][j] == '*') {
                // Cell is alive
                if (neighbors < 2 || neighbors > 3) {
                    // Dies from underpopulation or overpopulation
                    next_grid[i][j] = '.';
                } else {
                    // Survives
                    next_grid[i][j] = '*';
                }
            } else {
                // Cell is dead
                if (neighbors == 3) {
                    // Reproduction
                    next_grid[i][j] = '*';
                } else {
                    // Stays dead
                    next_grid[i][j] = '.';
                }
            }
        }
    }
    
    // Copy next_grid back to grid for the next iteration - also parallelized
    #pragma omp parallel for schedule(guided, 1)
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            grid[i][j] = next_grid[i][j];
        }
    }
}

// Count the number of live cells in the grid
int count_live_cells(char grid[GRID_SIZE][GRID_SIZE]) {
    int count = 0;
    
    #pragma omp parallel for reduction(+:count)
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            if (grid[i][j] == '*') {
                count++;
            }
        }
    }
    
    return count;
}

// Render the grid with improved visualization
void render_grid(SDL_Renderer *renderer, char grid[GRID_SIZE][GRID_SIZE], int live_count) {
    // Calculate colors based on live cell count
    // This creates a gradual color change as the simulation progresses
    int hue = (live_count * 360) / (GRID_SIZE * GRID_SIZE / 4);
    
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            SDL_Rect cell = {j * CELL_SIZE, i * CELL_SIZE, CELL_SIZE, CELL_SIZE};
            
            if (grid[i][j] == '*') {
                // Live cell - with color based on neighbor count
                int neighbors = count_neighbors(grid, i, j);
                
                // Color based on neighbor count
                switch (neighbors) {
                    case 0:
                    case 1:
                        // Underpopulated - reddish
                        SDL_SetRenderDrawColor(renderer, 255, 100, 100, 255);
                        break;
                    case 2:
                    case 3:
                        // Healthy - white
                        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                        break;
                    default:
                        // Overpopulated - bluish
                        SDL_SetRenderDrawColor(renderer, 100, 100, 255, 255);
                        break;
                }
                
                SDL_RenderFillRect(renderer, &cell);
                
                // Draw a border around live cells for better visibility
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 100);
                SDL_RenderDrawRect(renderer, &cell);
            } else {
                // Dead cell - dark gray (to show grid lines)
                SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
                SDL_RenderFillRect(renderer, &cell);
            }
        }
    }
}