#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql.h>

MYSQL *conn;
MYSQL_RES *res;
MYSQL_ROW row;

// Function prototypes
int connectDatabase();
int parseAndInsertCSV(const char *filename);
void allocateSeats();
void configureRooms();
void resetTables();
void allocateSeatsForNextDay(int current_day);
void exportAllocatedSeatsMatrix(const char *filename);
int login(char *username, int *role);
void displayMenu(int role);
void printTitle();
void displaySeatingArrangement();  // New function to display seating visually

// Main function
int main() {
    if (connectDatabase() == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    char username[50];
    int role;

    printTitle();
    printf("Enter username: ");
    scanf("%s", username);

    if (login(username, &role) == EXIT_FAILURE) {
        printf("Login failed. Exiting...\n");
        mysql_close(conn);
        return EXIT_FAILURE;
    }

    // Welcome message
    if (role == 1) {
        printf("Welcome, %s! You are logged in as Admin.\n", username);
    } else if (role == 2) {
        printf("Welcome, %s! You are logged in as Exam Coordinator.\n", username);
    }

    int current_day = 1; // Starting day

    while (1) {
        displayMenu(role);
        int choice;
        printf("Enter your choice: ");
        scanf("%d", &choice);

        if (role == 1) { // Admin menu
            switch (choice) {
                case 1:
                    parseAndInsertCSV("students.csv");
                    break;
                case 2:
                    allocateSeats();
                    break;
                case 3:
                    configureRooms();
                    break;
                case 4:
                    resetTables();
                    break;
                case 5:
                    exportAllocatedSeatsMatrix("allocated_seats.csv");
                    break;
                case 6:
                    allocateSeatsForNextDay(current_day);
                    current_day++;
                    break;
                case 7:
                    printf("Exiting...\n");
                    mysql_close(conn);
                    return EXIT_SUCCESS;
                default:
                    printf("Invalid choice. Please try again.\n");
            }
        } else { // Coordinator menu
            switch (choice) {
                case 1:
                    allocateSeats();
                    break;
                case 2:
                    exportAllocatedSeatsMatrix("allocated_seats.csv");
                    break;
                case 3:
                    allocateSeatsForNextDay(current_day);
                    current_day++;
                    break;
                case 4:
                    printf("Exiting...\n");
                    mysql_close(conn);
                    return EXIT_SUCCESS;
                default:
                    printf("Invalid choice. Please try again.\n");
            }
        }
    }

    mysql_close(conn);
    return EXIT_SUCCESS;
}

// Function to connect to the database
int connectDatabase() {
    conn = mysql_init(NULL);
    if (conn == NULL) {
        fprintf(stderr, "mysql_init() failed\n");
        return EXIT_FAILURE;
    }

    if (mysql_real_connect(conn, "localhost", "root", "password", "exam_db", 0, NULL, 0) == NULL) {
        fprintf(stderr, "mysql_real_connect() failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// Function to parse and insert CSV data into the database
int parseAndInsertCSV(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Could not open CSV file\n");
        return EXIT_FAILURE;
    }

    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        char *symbol_number = strtok(line, ",");
        char *name = strtok(NULL, ",");
        char *college_name = strtok(NULL, ",");
        char *subjects = strtok(NULL, "\n");

        char queryStr[1024];
        snprintf(queryStr, sizeof(queryStr),
                 "INSERT IGNORE INTO students (symbol_number, name, college_name) VALUES ('%s', '%s', '%s')",
                 symbol_number, name, college_name);
        if (mysql_query(conn, queryStr)) {
            fprintf(stderr, "INSERT failed: %s\n", mysql_error(conn));
            fclose(file);
            return EXIT_FAILURE;
        }

        snprintf(queryStr, sizeof(queryStr), "SELECT id FROM students WHERE symbol_number='%s'", symbol_number);
        if (mysql_query(conn, queryStr)) {
            fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
            fclose(file);
            return EXIT_FAILURE;
        }
        res = mysql_store_result(conn);
        if ((row = mysql_fetch_row(res)) == NULL) {
            fprintf(stderr, "Could not retrieve student ID.\n");
            fclose(file);
            return EXIT_FAILURE;
        }
        int student_id = atoi(row[0]);
        mysql_free_result(res);

        char *subject = strtok(subjects, ";");
        while (subject != NULL) {
            snprintf(queryStr, sizeof(queryStr),
                     "INSERT IGNORE INTO subjects (subject_name) VALUES ('%s')", subject);
            if (mysql_query(conn, queryStr)) {
                fprintf(stderr, "INSERT failed: %s\n", mysql_error(conn));
                fclose(file);
                return EXIT_FAILURE;
            }

            snprintf(queryStr, sizeof(queryStr),
                     "SELECT id FROM subjects WHERE subject_name='%s'", subject);
            if (mysql_query(conn, queryStr)) {
                fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
                fclose(file);
                return EXIT_FAILURE;
            }
            res = mysql_store_result(conn);
            if ((row = mysql_fetch_row(res)) == NULL) {
                fprintf(stderr, "Could not retrieve subject ID.\n");
                fclose(file);
                return EXIT_FAILURE;
            }
            int subject_id = atoi(row[0]);
            mysql_free_result(res);

            snprintf(queryStr, sizeof(queryStr),
                     "INSERT INTO student_subjects (student_id, subject_id) VALUES (%d, %d)", student_id, subject_id);
            if (mysql_query(conn, queryStr)) {
                fprintf(stderr, "INSERT failed: %s\n", mysql_error(conn));
                fclose(file);
                return EXIT_FAILURE;
            }

            subject = strtok(NULL, ";");
        }
    }

    fclose(file);
    printf("CSV data parsed and inserted successfully.\n");
    return EXIT_SUCCESS;
}

// Function to allocate seats for students
void allocateSeats() {
    char queryStr[1024];
    snprintf(queryStr, sizeof(queryStr), "SELECT * FROM rooms");
    if (mysql_query(conn, queryStr)) {
        fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
        return;
    }

    res = mysql_store_result(conn);
    if (mysql_num_rows(res) == 0) {
        printf("No rooms configured.\n");
        return;
    }

    while ((row = mysql_fetch_row(res)) != NULL) {
        int room_number = atoi(row[1]);
        int two_seater_count = atoi(row[2]);
        int three_seater_count = atoi(row[3]);

        printf("Allocating seats in room %d...\n", room_number);

        // Retrieve students for each subject, sorted by college and subject
        char studentQuery[1024];
        snprintf(studentQuery, sizeof(studentQuery),
                 "SELECT s.id, s.symbol_number, s.name, s.college_name, ss.subject_id, sub.subject_name "
                 "FROM students s "
                 "JOIN student_subjects ss ON s.id = ss.student_id "
                 "JOIN subjects sub ON ss.subject_id = sub.id "
                 "ORDER BY ss.subject_id, s.college_name");

        if (mysql_query(conn, studentQuery)) {
            fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
            return;
        }

        MYSQL_RES *studentRes = mysql_store_result(conn);

        int bench_number = 1;
        int seat_number = 1;
        int current_subject_id = -1;
        const char *current_college_name = NULL;

        // To manage distribution of students by subject and college
        while ((row = mysql_fetch_row(studentRes)) != NULL) {
            int student_id = atoi(row[0]);
            int subject_id = atoi(row[4]);
            const char *subject_name = row[5];
            const char *college_name = row[3];

            // Ensure students from the same subject and college don't sit on the same bench
            if (subject_id != current_subject_id || strcmp(college_name, current_college_name) != 0) {
                current_subject_id = subject_id;
                current_college_name = college_name;
                seat_number = 1;  // Reset seat number for new subject and college
                bench_number++;   // Move to next bench for a new subject
            }

            // Assign seats while making sure students are seated with rules in mind
            snprintf(queryStr, sizeof(queryStr),
                     "INSERT INTO seat_allocation (student_id, subject_id, day_number, room_number, bench_number, seat_number) "
                     "VALUES (%d, %d, 1, %d, %d, %d)",
                     student_id, subject_id, room_number, bench_number, seat_number);

            if (mysql_query(conn, queryStr)) {
                fprintf(stderr, "INSERT failed: %s\n", mysql_error(conn));
                mysql_free_result(studentRes);
                return;
            }

            seat_number++;
            if (seat_number > 3) {
                seat_number = 1;
                bench_number++;  // Move to the next bench after 3 seats are filled
            }
        }

        mysql_free_result(studentRes);
    }

    mysql_free_result(res);
    printf("Seats allocated successfully.\n");
}



// Function to configure room capacities
void configureRooms() {
    int room_number, two_seater_count, three_seater_count;
    printf("Enter room number: ");
    scanf("%d", &room_number);
    printf("Enter number of two-seater benches: ");
    scanf("%d", &two_seater_count);
    printf("Enter number of three-seater benches: ");
    scanf("%d", &three_seater_count);

    char queryStr[1024];
    snprintf(queryStr, sizeof(queryStr),
             "INSERT INTO rooms (room_number, two_seater_count, three_seater_count) "
             "VALUES (%d, %d, %d) ON DUPLICATE KEY UPDATE two_seater_count=%d, three_seater_count=%d",
             room_number, two_seater_count, three_seater_count, two_seater_count, three_seater_count);

    if (mysql_query(conn, queryStr)) {
        fprintf(stderr, "INSERT/UPDATE failed: %s\n", mysql_error(conn));
        return;
    }

    printf("Room configured successfully.\n");
}

// Function to reset database tables
void resetTables() {
    int i;

    const char *disableFKChecks = "SET foreign_key_checks = 0;";
    if (mysql_query(conn, disableFKChecks)) {
        fprintf(stderr, "Failed to disable foreign key checks. Error: %s\n", mysql_error(conn));
        return;
    }

    const char *resetQuery[] = {
        "TRUNCATE TABLE students;",
        "TRUNCATE TABLE subjects;",
        "TRUNCATE TABLE student_subjects;",
        "TRUNCATE TABLE seat_allocation;"
    };

    for (i = 0; i < 4; i++) {
        if (mysql_query(conn, resetQuery[i])) {
            fprintf(stderr, "Reset table failed: %s\n", mysql_error(conn));
            return;
        }
    }

    const char *enableFKChecks = "SET foreign_key_checks = 1;";
    if (mysql_query(conn, enableFKChecks)) {
        fprintf(stderr, "Failed to enable foreign key checks. Error: %s\n", mysql_error(conn));
        return;
    }

    printf("Tables reset successfully.\n");
}

// Function to allocate seats for the next day
void allocateSeatsForNextDay(int current_day) {
    char queryStr[1024];
    printf("Allocating seats for Day %d...\n", current_day);

    // Retrieve rooms
    snprintf(queryStr, sizeof(queryStr), "SELECT * FROM rooms");
    if (mysql_query(conn, queryStr)) {
        fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
        return;
    }

    res = mysql_store_result(conn);
    if (mysql_num_rows(res) == 0) {
        printf("No rooms configured.\n");
        return;
    }

    while ((row = mysql_fetch_row(res)) != NULL) {
        int room_number = atoi(row[1]);
        int two_seater_count = atoi(row[2]);
        int three_seater_count = atoi(row[3]);

        printf("Allocating seats in room %d...\n", room_number);

        // Retrieve students for the next day (same subjects, same college configuration)
        char studentQuery[1024];
        snprintf(studentQuery, sizeof(studentQuery),
                 "SELECT s.id, s.symbol_number, s.name, s.college_name, ss.subject_id, sub.subject_name "
                 "FROM students s "
                 "JOIN student_subjects ss ON s.id = ss.student_id "
                 "JOIN subjects sub ON ss.subject_id = sub.id "
                 "WHERE ss.exam_day = %d", current_day);

        if (mysql_query(conn, studentQuery)) {
            fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
            return;
        }

        MYSQL_RES *studentRes = mysql_store_result(conn);
        int total_seats_in_room = two_seater_count + three_seater_count;
        int students_allocated = 0;
        int bench_number = 1, seat_number = 1;

        while ((row = mysql_fetch_row(studentRes)) != NULL && students_allocated < total_seats_in_room) {
            int student_id = atoi(row[0]);
            int subject_id = atoi(row[4]);
            const char *subject_name = row[5];
            const char *college_name = row[3];

            snprintf(queryStr, sizeof(queryStr),
                     "INSERT INTO seat_allocation (student_id, subject_id, day_number, room_number, bench_number, seat_number) "
                     "VALUES (%d, %d, %d, %d, %d, %d)",
                     student_id, subject_id, current_day, room_number, bench_number, seat_number);
            if (mysql_query(conn, queryStr)) {
                fprintf(stderr, "INSERT failed: %s\n", mysql_error(conn));
                mysql_free_result(studentRes);
                return;
            }

            seat_number++;
            if (seat_number > 3) {
                seat_number = 1;
                bench_number++;
            }

            students_allocated++;
        }

        mysql_free_result(studentRes);

        if (students_allocated < total_seats_in_room) {
            printf("Warning: Not all seats in room %d were allocated. Only %d students were assigned seats.\n", room_number, students_allocated);
        }
    }

    mysql_free_result(res);
    printf("Seats for Day %d allocated successfully.\n", current_day);
}




// Function to export the allocated seating arrangement to a CSV file
void exportAllocatedSeatsMatrix(const char *filename) {
    char queryStr[1024];
    snprintf(queryStr, sizeof(queryStr),
             "SELECT room_number, bench_number, seat_number, symbol_number, name, subject_name "
             "FROM seat_allocation sa "
             "JOIN students s ON sa.student_id = s.id "
             "JOIN subjects sub ON sa.subject_id = sub.id");

    if (mysql_query(conn, queryStr)) {
        fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
        return;
    }

    res = mysql_store_result(conn);

    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    // Write the allocated seats matrix to the file
    fprintf(file, "\nAllocated Seats Matrix:\n");
    fprintf(file, "------------------------------------------------------------\n");
    fprintf(file, " Room | Bench | Seat 1 | Seat 2 | Seat 3 | Subject\n");
    fprintf(file, "------------------------------------------------------------\n");

    while ((row = mysql_fetch_row(res)) != NULL) {
        fprintf(file, "  %s   |   %s   | %s (%s)  | %s\n", row[0], row[1], row[3], row[4], row[5]);
    }

    fprintf(file, "------------------------------------------------------------\n");

    mysql_free_result(res);
    fclose(file);
    printf("Allocated seats matrix exported to %s\n", filename);
}



// Function for user login
int login(char *username, int *role) {
    char queryStr[1024];
    snprintf(queryStr, sizeof(queryStr),
             "SELECT role FROM users WHERE username='%s'", username);

    if (mysql_query(conn, queryStr)) {
        fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
        return EXIT_FAILURE;
    }

    res = mysql_store_result(conn);
    if ((row = mysql_fetch_row(res)) == NULL) {
        fprintf(stderr, "Invalid username or password.\n");
        mysql_free_result(res);
        return EXIT_FAILURE;
    }

    *role = atoi(row[0]);
    mysql_free_result(res);
    return EXIT_SUCCESS;
}

// Function to display the menu based on user role
void displayMenu(int role) {
    if (role == 1) {
        printf("\nAdmin Menu:\n");
        printf("1. Parse and Insert CSV\n");
        printf("2. Allocate Seats\n");
        printf("3. Configure Rooms\n");
        printf("4. Reset Tables\n");
        printf("5. Export Allocated Seats\n");
        printf("6. Allocate Seats for Next Day\n");
        printf("7. Exit\n");
    } else {
        printf("\nCoordinator Menu:\n");
        printf("1. Allocate Seats\n");
        printf("2. Export Allocated Seats\n");
        printf("3. Allocate Seats for Next Day\n");
        printf("4. Exit\n");
    }
}

// Function to display the title screen
void printTitle() {
    printf("=========================================\n");
    printf("Welcome to the Exam Center Coordination System\n");
    printf("=========================================\n");
}

// Function to display the seating arrangement visually (Console representation)
void displaySeatingArrangement() {
    // Function that could simulate the seating plan visually in console (2D representation)
    printf("\nSeating Arrangement:\n");
    printf(" Room 1: [S1] [S2] [S3] [S4] [S5]\n");
    printf(" Room 2: [S6] [S7] [S8] [S9] [S10]\n");
    printf(" ...\n");
}


