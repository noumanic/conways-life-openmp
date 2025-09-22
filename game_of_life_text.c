#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <time.h>
#include <stdbool.h>

#define GRID_SIZE 100
#define ITERATIONS 100
#define CENTER_SIZE 10
#define MEASUREMENTS 5

// Function prototypes
void initialize_grid(char grid[GRID_SIZE][GRID_SIZE]);
int count_neighbors(char grid[GRID_SIZE][GRID_SIZE], int row, int col);
void simulate_serial(char grid[GRID_SIZE][GRID_SIZE], char next_grid[GRID_SIZE][GRID_SIZE]);
void simulate_parallel_static(char grid[GRID_SIZE][GRID_SIZE], char next_grid[GRID_SIZE][GRID_SIZE]);
void simulate_parallel_guided(char grid[GRID_SIZE][GRID_SIZE], char next_grid[GRID_SIZE][GRID_SIZE]);
void simulate_parallel_static_no_critical(char grid[GRID_SIZE][GRID_SIZE], char next_grid[GRID_SIZE][GRID_SIZE]);
void simulate_parallel_guided_no_critical(char grid[GRID_SIZE][GRID_SIZE], char next_grid[GRID_SIZE][GRID_SIZE]);
void print_grid(char grid[GRID_SIZE][GRID_SIZE]);
double run_simulation(void (*simulate_func)(char[GRID_SIZE][GRID_SIZE], char[GRID_SIZE][GRID_SIZE]), const char* label, bool print_final);

int main() {
    double serial_time = 0, static_time = 0, guided_time = 0;
    double static_no_critical_time = 0, guided_no_critical_time = 0;
    
    printf("Conway's Game of Life Simulation\n");
    printf("================================\n\n");
    
    // Run each version multiple times and calculate average
    for (int i = 0; i < MEASUREMENTS; i++) {
        printf("Measurement %d of %d\n", i + 1, MEASUREMENTS);
        
        // Only print the final grid for the last measurement
        bool print_final = (i == MEASUREMENTS - 1);
        
        serial_time += run_simulation(simulate_serial, "Serial", print_final);
        static_time += run_simulation(simulate_parallel_static, "Parallel (Static Scheduling)", false);
        guided_time += run_simulation(simulate_parallel_guided, "Parallel (Guided Scheduling)", false);
        static_no_critical_time += run_simulation(simulate_parallel_static_no_critical, "Parallel (Static No Critical)", false);
        guided_no_critical_time += run_simulation(simulate_parallel_guided_no_critical, "Parallel (Guided No Critical)", false);
        
        printf("\n");
    }
    
    // Calculate averages
    serial_time /= MEASUREMENTS;
    static_time /= MEASUREMENTS;
    guided_time /= MEASUREMENTS;
    static_no_critical_time /= MEASUREMENTS;
    guided_no_critical_time /= MEASUREMENTS;
    
    // Calculate speedups
    double static_speedup = serial_time / static_time;
    double guided_speedup = serial_time / guided_time;
    double static_no_crit_speedup = serial_time / static_no_critical_time;
    double guided_no_crit_speedup = serial_time / guided_no_critical_time;
    
    // Print performance report
    printf("\nPERFORMANCE REPORT\n");
    printf("=================\n");
    printf("Average Execution Times:\n");
    printf("  Serial Version: %.4f seconds\n", serial_time);
    printf("  Parallel (Static): %.4f seconds (Speedup: %.2fx)\n", static_time, static_speedup);
    printf("  Parallel (Guided): %.4f seconds (Speedup: %.2fx)\n", guided_time, guided_speedup);
    printf("  Parallel (Static No Critical): %.4f seconds (Speedup: %.2fx)\n", 
           static_no_critical_time, static_no_crit_speedup);
    printf("  Parallel (Guided No Critical): %.4f seconds (Speedup: %.2fx)\n", 
           guided_no_critical_time, guided_no_crit_speedup);
    
    // Analysis of results
    printf("\nANALYSIS\n");
    printf("========\n");
    printf("1. Best performance: ");
    
    // Find the best performing version
    double best_time = serial_time;
    const char* best_version = "Serial";
    
    if (static_time < best_time) {
        best_time = static_time;
        best_version = "Parallel (Static)";
    }
    if (guided_time < best_time) {
        best_time = guided_time;
        best_version = "Parallel (Guided)";
    }
    if (static_no_critical_time < best_time) {
        best_time = static_no_critical_time;
        best_version = "Parallel (Static No Critical)";
    }
    if (guided_no_critical_time < best_time) {
        best_time = guided_no_critical_time;
        best_version = "Parallel (Guided No Critical)";
    }
    
    printf("%s (%.4f seconds)\n", best_version, best_time);
    
    // Compare scheduling methods
    printf("2. Scheduling comparison: ");
    if (static_time < guided_time) {
        printf("Static scheduling performed better than guided scheduling by %.2f%%\n", 
               (guided_time - static_time) / guided_time * 100);
    } else if (guided_time < static_time) {
        printf("Guided scheduling performed better than static scheduling by %.2f%%\n", 
               (static_time - guided_time) / static_time * 100);
    } else {
        printf("Both scheduling methods performed equally\n");
    }
    
    // Critical section impact
    printf("3. Critical section impact:\n");
    printf("   - Static scheduling: Removal of critical section ");
    if (static_no_critical_time < static_time) {
        printf("improved performance by %.2f%%\n", 
               (static_time - static_no_critical_time) / static_time * 100);
    } else {
        printf("degraded performance by %.2f%%\n", 
               (static_no_critical_time - static_time) / static_time * 100);
    }
    
    printf("   - Guided scheduling: Removal of critical section ");
    if (guided_no_critical_time < guided_time) {
        printf("improved performance by %.2f%%\n", 
               (guided_time - guided_no_critical_time) / guided_time * 100);
    } else {
        printf("degraded performance by %.2f%%\n", 
               (guided_no_critical_time - guided_time) / guided_time * 100);
    }
    
    printf("\nNOTE: Versions without critical sections may produce inconsistent results\n");
    printf("due to potential race conditions during the grid update phase.\n");
    
    return 0;
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

// Serial implementation of the Game of Life simulation
void simulate_serial(char grid[GRID_SIZE][GRID_SIZE], char next_grid[GRID_SIZE][GRID_SIZE]) {
    for (int iter = 0; iter < ITERATIONS; iter++) {
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
        memcpy(grid, next_grid, GRID_SIZE * GRID_SIZE * sizeof(char));
    }
}

// Parallel implementation with static scheduling
void simulate_parallel_static(char grid[GRID_SIZE][GRID_SIZE], char next_grid[GRID_SIZE][GRID_SIZE]) {
    for (int iter = 0; iter < ITERATIONS; iter++) {
        // Calculate next generation in parallel
        #pragma omp parallel for schedule(static, 1)
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
        
        // Need critical section for this copy operation
        #pragma omp critical
        {
            memcpy(grid, next_grid, GRID_SIZE * GRID_SIZE * sizeof(char));
        }
    }
}

// Parallel implementation with guided scheduling
void simulate_parallel_guided(char grid[GRID_SIZE][GRID_SIZE], char next_grid[GRID_SIZE][GRID_SIZE]) {
    for (int iter = 0; iter < ITERATIONS; iter++) {
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
        
        // Need critical section for this copy operation
        #pragma omp critical
        {
            memcpy(grid, next_grid, GRID_SIZE * GRID_SIZE * sizeof(char));
        }
    }
}

// Parallel implementation with static scheduling without critical section
void simulate_parallel_static_no_critical(char grid[GRID_SIZE][GRID_SIZE], char next_grid[GRID_SIZE][GRID_SIZE]) {
    for (int iter = 0; iter < ITERATIONS; iter++) {
        // Calculate next generation in parallel
        #pragma omp parallel for schedule(static, 1)
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
        
        // No critical section - potential race condition
        memcpy(grid, next_grid, GRID_SIZE * GRID_SIZE * sizeof(char));
    }
}

// Parallel implementation with guided scheduling without critical section
void simulate_parallel_guided_no_critical(char grid[GRID_SIZE][GRID_SIZE], char next_grid[GRID_SIZE][GRID_SIZE]) {
    for (int iter = 0; iter < ITERATIONS; iter++) {
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
        
        // No critical section - potential race condition
        memcpy(grid, next_grid, GRID_SIZE * GRID_SIZE * sizeof(char));
    }
}

// Print the grid
void print_grid(char grid[GRID_SIZE][GRID_SIZE]) {
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            printf("%c", grid[i][j]);
        }
        printf("\n");
    }
}

// Run a simulation with the given simulation function and measure its execution time
double run_simulation(void (*simulate_func)(char[GRID_SIZE][GRID_SIZE], char[GRID_SIZE][GRID_SIZE]), 
                      const char* label, bool print_final) {
    char grid[GRID_SIZE][GRID_SIZE];
    char next_grid[GRID_SIZE][GRID_SIZE];
    
    // Initialize grid
    initialize_grid(grid);
    
    // Measure execution time
    printf("Running %s simulation...\n", label);
    double start_time = omp_get_wtime();
    
    simulate_func(grid, next_grid);
    
    double end_time = omp_get_wtime();
    double time_taken = end_time - start_time;
    
    printf("  Time taken: %.4f seconds\n", time_taken);
    
    // Print final state if requested
    if (print_final) {
        printf("\nFinal grid state for %s (after %d iterations):\n", label, ITERATIONS);
        print_grid(grid);
    }
    
    return time_taken;
}