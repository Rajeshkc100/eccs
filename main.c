#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql.h>
#include <conio.h>
#include <ctype.h>
#ifdef _WIN32
    #include <windows.h> // For Windows
#else
    #include <unistd.h>  // For Linux/macOS
#endif

// Global variables
MYSQL *conn;
MYSQL_RES *res;
MYSQL_ROW row;

// Struct for room data
typedef struct {
    int room_id;
    int room_number;
    int total_benches;
    int seats_per_bench;
    int **seats; // 2D array for seat status
} Room;

// ==== Function prototypes ====
// Database-related functions
int connectDatabase();
void createDefaultAdminUser();  // Prototype added here
int parseAndInsertCSV(const char *filename);


// Room management functions
void configureRooms();
void resetTables();

// Seat allocation functions
void unifiedSeatAllocation(int maxDays);
void allocateSeatsForDay(int day);
void allocateSeatsForResult(MYSQL_RES *result, int day);
int isAdjacentSeatConflict(int room_id, int bench, int seat, int total_benches, int seats_per_bench, int subject_id, int **seatMatrix);
void allocateSeat(int student_id, int subject_id, Room *room, int bench, int seat, int day);

// User management functions
void getPassword(char *password, size_t size);
int registerUser();
int login(char *username, int *role);

// Menu and display functions
void displayMenu(int role);
void printTitle();
void clearScreenWithMessage(const char *message);

// Input validation function
int getValidatedChoice(const char *prompt);

// Export-related functions
void exportAllocatedSeatsMatrix(const char *filename);


// Main function
int main() {
    if (connectDatabase() == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    char username[50];
    int role;
    int maxDays = 4; // Maximum number of exam days

    printTitle();
    printf("Enter username: ");
    scanf("%s", username);

    if (login(username, &role) == EXIT_FAILURE) {
        printf("\nLogin failed. Exiting...\n");
        mysql_close(conn);
        return EXIT_FAILURE;
    }

    printf("\nWelcome, %s!\n", (role == 1) ? "Admin" : "Exam Coordinator");

    while (1) {
        clearScreenWithMessage("\nLoading Menu...");
        displayMenu(role);
        int choice = getValidatedChoice("\nEnter your choice: ");

        if (role == 1) { // Admin menu
            switch (choice) {
                case 1:
                    parseAndInsertCSV("students.csv");
                    break;
                case 2:
                	configureRooms();
                    break;
                case 3:
                    unifiedSeatAllocation(maxDays);
                    break;
                case 4:
                    resetTables();
                    break;
                case 5:
                    exportAllocatedSeatsMatrix("seat_allocation.csv");
                    break;
                case 6:
                    if (registerUser() == EXIT_FAILURE) {
                        printf("User registration failed.\n");
                    }
                    break;
                case 7:
                    printf("Exiting...\n");
                    mysql_close(conn);
                    return EXIT_SUCCESS;
                default:
                    printf("Invalid choice. Try again.\n");
            }
        } else { // Coordinator menu
            switch (choice) {
                case 1:
                    unifiedSeatAllocation(maxDays);
                    break;
                case 2:
                    exportAllocatedSeatsMatrix("seat_allocation.csv");
                    break;
                case 3:
                    printf("\nExiting...\n");
                    mysql_close(conn);
                    return EXIT_SUCCESS;
                default:
                    printf("Invalid choice. Try again.\n");
            }
        }
    }

    mysql_close(conn);
    return EXIT_SUCCESS;
}

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
    
    // Create the default admin user
    createDefaultAdminUser();
    
    return EXIT_SUCCESS;
}

void createDefaultAdminUser() {
    // SQL query to insert the default admin user into the users table
    // Username: 'admin', Password: 'admin' (hashed using MD5), Role: 1 (Admin)
    char query[256];
    snprintf(query, sizeof(query),
             "INSERT IGNORE INTO users (username, password, role) "
             "VALUES ('admin', MD5('admin'), 1)");

    // Execute the query
    if (mysql_query(conn, query)) {
        // Log an error if the query execution fails
        fprintf(stderr, "Error creating default admin user: %s\n", mysql_error(conn));
    } else {
        // Log success message if the admin user is created or already exists
        printf("Default admin user initialized (username: admin, password: admin).\n");
    }
}


// Function to validate numeric input
int getValidatedChoice(const char *prompt) {
    char input[50];
    int number;

    while (1) {
        printf("%s", prompt);
        scanf("%s", input);

        // Check if input contains only digits
        int isValid = 1;
        int i;
        for (i = 0; input[i] != '\0'; i++) {
            if (!isdigit(input[i])) {
                isValid = 0;
                break;
            }
        }

        if (isValid) {
            number = atoi(input); // Convert valid string to number
            break;
        } else {
            printf("Invalid input! Please enter a numeric value.\n");
        }
    }

    return number;
}

void clearScreenWithMessage(const char *message) {
    // Display the message
    printf("%s\n", message);

    // Pause for 2 seconds
    #ifdef _WIN32
        Sleep(1000); // Sleep in milliseconds
    #else
        sleep(1);    // Sleep in seconds
    #endif

    // Clear the screen based on the operating system
    #ifdef _WIN32
        system("cls");  // Windows
    #else
        system("clear"); // Linux/macOS
    #endif
}

void printTitle() {
    printf("\n================================================\n");
    printf(" Welcome to the Exam Center Coordination System \n");
    printf("================================================\n\n");
	clearScreenWithMessage("Starting the system... Please wait!");
}

void displayMenu(int role) {
    if (role == 1) { // Admin menu
        printf("\nAdmin Menu:\n");
        printf("1. Parse and Insert CSV Data\n");
        printf("2. Configure Rooms\n");
        printf("3. Allocate Seats\n");
        printf("4. Reset Tables\n");
        printf("5. Export Allocated Seats\n");
        printf("6. Register New User\n");
        printf("7. Exit\n");
    } else { // Coordinator menu
        printf("\nExam Coordinator Menu:\n");
        printf("1. Allocate Seats\n");
        printf("2. Export Allocated Seats\n");
        printf("3. Exit\n");
    }
}

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
    printf("\nCSV data parsed and inserted successfully.\n");
    return EXIT_SUCCESS;
}


void configureRooms() {
    char queryStr[256];
    int id, twoSeaterCount, threeSeaterCount;
    char choice;

    printf("\nConfiguring Rooms:\n");
    printf("===================\n");

    // Fetch existing room configurations
    if (mysql_query(conn, "SELECT id, room_number, two_seater_count, three_seater_count FROM rooms")) {
        fprintf(stderr, "Query failed: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_RES *result = mysql_store_result(conn);
    if (result == NULL) {
        fprintf(stderr, "Could not retrieve rooms data: %s\n", mysql_error(conn));
        return;
    }

    printf("\nCurrent Room Configurations:\n");
    printf("ID | Room Number | Two-Seater Count | Three-Seater Count\n");
    printf("-------------------------------------------------------\n");
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        printf("%-2s | %-11s | %-16s | %-18s\n", row[0], row[1], row[2], row[3]);
    }
    mysql_free_result(result);

    while (1) {
        printf("\nDo you want to (A)dd, (E)dit, or (D)elete a room? (Enter Q to quit): ");
        scanf(" %c", &choice);

        if (choice == 'Q' || choice == 'q') {
            break;
        }

        switch (choice) {
            case 'A':
            case 'a': {
                int roomNumber;
                printf("\nEnter Room Number: ");
                scanf("%d", &roomNumber);
                printf("Enter Two-Seater Count: ");
                scanf("%d", &twoSeaterCount);
                printf("Enter Three-Seater Count: ");
                scanf("%d", &threeSeaterCount);

                snprintf(queryStr, sizeof(queryStr),
                         "INSERT INTO rooms (room_number, two_seater_count, three_seater_count) "
                         "VALUES (%d, %d, %d)", roomNumber, twoSeaterCount, threeSeaterCount);

                if (mysql_query(conn, queryStr)) {
                    fprintf(stderr, "Error adding room: %s\n", mysql_error(conn));
                } else {
                    printf("Room added successfully.\n");
                }
                break;
            }

            case 'E':
            case 'e': {
                printf("\nEnter Room ID to Edit: ");
                scanf("%d", &id);

                printf("Enter New Two-Seater Count: ");
                scanf("%d", &twoSeaterCount);
                printf("Enter New Three-Seater Count: ");
                scanf("%d", &threeSeaterCount);

                snprintf(queryStr, sizeof(queryStr),
                         "UPDATE rooms SET two_seater_count = %d, three_seater_count = %d "
                         "WHERE id = %d", twoSeaterCount, threeSeaterCount, id);

                if (mysql_query(conn, queryStr)) {
                    fprintf(stderr, "Error updating room: %s\n", mysql_error(conn));
                } else {
                    printf("Room updated successfully.\n");
                }
                break;
            }

            case 'D':
            case 'd': {
                printf("\nEnter Room ID to Delete: ");
                scanf("%d", &id);

                snprintf(queryStr, sizeof(queryStr), "DELETE FROM rooms WHERE id = %d", id);

                if (mysql_query(conn, queryStr)) {
                    fprintf(stderr, "Error deleting room: %s\n", mysql_error(conn));
                } else {
                    printf("\nRoom deleted successfully.\n");
                }
                break;
            }

            default:
                printf("\nInvalid choice. Please try again.\n");
        }
    }

    printf("\nExiting Room Configuration.\n");
}


void resetTables() {
    const char *queries[] = {
        "SET FOREIGN_KEY_CHECKS = 0",
        "TRUNCATE TABLE seat_allocation",
        "TRUNCATE TABLE student_subjects",
        "TRUNCATE TABLE students",
        "TRUNCATE TABLE subjects",
        "TRUNCATE TABLE rooms",
        "SET FOREIGN_KEY_CHECKS = 1"
    };
    int numQueries = sizeof(queries) / sizeof(queries[0]);
    int i;

    for (i = 0; i < numQueries; i++) {
        if (mysql_query(conn, queries[i])) {
            fprintf(stderr, "Query failed: %s\n", mysql_error(conn));
            return;
        }
    }

    printf("\nAll tables have been reset successfully.\n");
    clearScreenWithMessage("...");
}

void unifiedSeatAllocation(int maxDays) {
    char queryStr[512];
    snprintf(queryStr, sizeof(queryStr),
             "SELECT MAX(subject_count) FROM (SELECT COUNT(ss.subject_id) AS subject_count "
             "FROM student_subjects ss GROUP BY ss.student_id) AS subject_counts");

    if (mysql_query(conn, queryStr)) {
        fprintf(stderr, "Query to fetch maximum subject count failed: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_RES *result = mysql_store_result(conn);
    if (result == NULL) {
        fprintf(stderr, "Could not retrieve subject count: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    int maxSubjects = row ? atoi(row[0]) : 0;
    mysql_free_result(result);

    if (maxSubjects == 0) {
        printf("No subjects found for any student.\n");
        return;
    }

    // Iterate through days up to the maximum allowed or required
    int day;
    for (day = 1; day <= maxDays && day <= maxSubjects; day++) {
        printf("\nAllocating seats for Day %d...\n", day);
        allocateSeatsForDay(day);
    }
}

// Function to allocate a seat
void allocateSeat(int student_id, int subject_id, Room *room, int bench, int seat, int day) {
    char query[1024];

    // Insert the allocation into the seat_allocation table
    snprintf(query, sizeof(query),
             "INSERT INTO seat_allocation (student_id, subject_id, room_id, bench_number, seat_number, day) "
             "VALUES (%d, %d, %d, %d, %d, %d)",
             student_id, subject_id, room->room_id, bench, seat, day);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "Seat allocation failed for student_id=%d, subject_id=%d on Day %d: %s\n",
                student_id, subject_id, day, mysql_error(conn));
        return;
    }

    // Mark the seat as allocated in the room's seat matrix
    room->seats[bench][seat] = subject_id;
}

// Main allocation function
void allocateSeatsForAllDays() {
    char query[512];
    MYSQL_RES *result;
    MYSQL_ROW row;

    // Query to determine the maximum number of subjects for any student
    snprintf(query, sizeof(query),
             "SELECT MAX(subject_count) FROM ("
             "SELECT COUNT(subject_id) AS subject_count FROM student_subjects GROUP BY student_id"
             ") AS subject_counts");

    if (mysql_query(conn, query)) {
        fprintf(stderr, "Query to fetch maximum subject count failed: %s\n", mysql_error(conn));
        return;
    }

    result = mysql_store_result(conn);
    if (result == NULL) {
        fprintf(stderr, "Could not retrieve subject count: %s\n", mysql_error(conn));
        return;
    }

    row = mysql_fetch_row(result);
    int maxDays = row ? atoi(row[0]) : 0;  // Maximum number of subjects = Number of days
    mysql_free_result(result);

    if (maxDays == 0) {
        printf("No subjects found for any student. No allocation needed.\n");
        return;
    }

    printf("Allocating seats for %d days based on the maximum number of subjects.\n", maxDays);

    // Iterate over days and allocate seats
    int day;
    for (day = 1; day <= maxDays; day++) {
        printf("\nAllocating seats for Day %d...\n", day);
        allocateSeatsForDay(day);  // Allocate seats for the current day
        printf("\nDay %d allocation completed.\n", day);
    }

    printf("\nAll %d days processed. Seat allocation completed.\n", maxDays);
}


void allocateSeatsForDay(int day) {
    char query[2048];
    MYSQL_RES *result;
    MYSQL_ROW row;

    // Fetch students and their subjects for the given day (subject_index = day)
    snprintf(query, sizeof(query),
             "SELECT s.id AS student_id, ss.subject_id "
             "FROM students s "
             "JOIN student_subjects ss ON s.id = ss.student_id "
             "WHERE ss.subject_index = %d "
             "AND NOT EXISTS (SELECT 1 FROM seat_allocation "
             "WHERE seat_allocation.student_id = s.id "
             "AND seat_allocation.subject_id = ss.subject_id "
             "AND seat_allocation.day = %d) "
             "ORDER BY s.id, ss.subject_id", day, day);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "Query failed: %s\n", mysql_error(conn));
        return;
    }

    result = mysql_store_result(conn);
    if (result == NULL) {
        fprintf(stderr, "Could not retrieve result set: %s\n", mysql_error(conn));
        return;
    }

    // Room and seat allocation logic remains the same
    // Use the revised function from earlier to allocate seats for students
    allocateSeatsForResult(result, day);  // Helper function to allocate seats
    mysql_free_result(result);
}

void allocateSeatsForResult(MYSQL_RES *result, int day) {
    MYSQL_ROW row;

    // Query to fetch room details
    char query[512];
    snprintf(query, sizeof(query),
             "SELECT id, room_number, two_seater_count, three_seater_count "
             "FROM rooms");

    if (mysql_query(conn, query)) {
        fprintf(stderr, "Room query failed: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_RES *roomResult = mysql_store_result(conn);
    if (roomResult == NULL) {
        fprintf(stderr, "Could not retrieve room data: %s\n", mysql_error(conn));
        return;
    }

    int roomCount = mysql_num_rows(roomResult);
    Room *rooms = malloc(sizeof(Room) * roomCount);
    if (!rooms) {
        fprintf(stderr, "Memory allocation failed for rooms.\n");
        mysql_free_result(roomResult);
        return;
    }

    // Initialize room data
    int roomIndex = 0;
    MYSQL_ROW roomRow;
    while ((roomRow = mysql_fetch_row(roomResult))) {
        rooms[roomIndex].room_id = atoi(roomRow[0]);
        rooms[roomIndex].room_number = atoi(roomRow[1]);
        int twoSeaterCount = atoi(roomRow[2]);
        int threeSeaterCount = atoi(roomRow[3]);
        rooms[roomIndex].total_benches = twoSeaterCount + threeSeaterCount;

        // Allocate memory for seat status
        rooms[roomIndex].seats = malloc(sizeof(int *) * rooms[roomIndex].total_benches);
        int i;
		for (i = 0; i < rooms[roomIndex].total_benches; i++) {
            int seats_per_bench = (i < twoSeaterCount) ? 2 : 3; // Determine bench type
            rooms[roomIndex].seats[i] = calloc(seats_per_bench, sizeof(int));
            if (!rooms[roomIndex].seats[i]) {
                fprintf(stderr, "Memory allocation failed for bench %d in room %d.\n", i, roomIndex);
                exit(EXIT_FAILURE);
            }
        }
        roomIndex++;
    }
    mysql_free_result(roomResult);

    // Allocate seats for students
    int total_allocated = 0, total_failed = 0;
    while ((row = mysql_fetch_row(result))) {
        int student_id = atoi(row[0]);
        int subject_id = atoi(row[1]);
        int allocated = 0;

        // Try to allocate a seat for this student and subject
        int i, bench, seat;
        for (i = 0; i < roomCount && !allocated; i++) {
            Room *room = &rooms[i];
            for (bench = 0; bench < room->total_benches && !allocated; bench++) {
                int seats_per_bench = (bench < room->total_benches / 2) ? 2 : 3; // Determine seats per bench dynamically
                for (seat = 0; seat < seats_per_bench && !allocated; seat++) {
                    if (room->seats[bench][seat] == 0 &&  // Seat is free
                        !isAdjacentSeatConflict(room->room_id, bench, seat, room->total_benches,
                                                seats_per_bench, subject_id, room->seats)) {
                        allocateSeat(student_id, subject_id, room, bench, seat, day);
                        room->seats[bench][seat] = subject_id;  // Mark seat as allocated
                        allocated = 1;
                        total_allocated++;
                    }
                }
            }
        }

        if (!allocated) {
            printf("Failed to allocate seat for student %d (Subject: %d) on Day %d.\n",
                   student_id, subject_id, day);
            total_failed++;
        }
    }

    printf("\nDay %d Allocation Summary:\n", day);
    printf("Total Allocated: %d\n", total_allocated);
    printf("Total Failed: %d\n", total_failed);

    // Free memory for rooms and benches
    int i, j;
    for (i = 0; i < roomCount; i++) {
        for (j = 0; j < rooms[i].total_benches; j++) {
            free(rooms[i].seats[j]);
        }
        free(rooms[i].seats);
    }
    free(rooms);
}

int isAdjacentSeatConflict(int room_id, int bench, int seat, int total_benches, int seats_per_bench, int subject_id, int **seatMatrix) {
    // Check the current bench
    int i;
    for (i = -1; i <= 1; i++) {  // Check adjacent seats: -1 (left), 0 (current), 1 (right)
        int adjacentSeat = seat + i;
        if (adjacentSeat >= 0 && adjacentSeat < seats_per_bench && seatMatrix[bench][adjacentSeat] == subject_id) {
            return 1;  // Conflict detected
        }
    }

    // Check the previous bench
    if (bench > 0) {  // Ensure the previous bench exists
        for (i = 0; i < seats_per_bench; i++) {
            if (seatMatrix[bench - 1][i] == subject_id) {
                return 1;  // Conflict detected
            }
        }
    }

    // Check the next bench
    if (bench < total_benches - 1) {  // Ensure the next bench exists
        for (i = 0; i < seats_per_bench; i++) {
            if (seatMatrix[bench + 1][i] == subject_id) {
                return 1;  // Conflict detected
            }
        }
    }

    return 0;  // No conflict
}

void exportAllocatedSeatsMatrix(const char *filename) {
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        fprintf(stderr, "Could not open file for writing.\n");
        return;
    }

    // Query to fetch seat allocation details
    char queryStr[1024];
    snprintf(queryStr, sizeof(queryStr),
             "SELECT a.day, r.room_number, a.bench_number, r.two_seater_count, r.three_seater_count, "
             "a.seat_number, s.symbol_number, sub.subject_name "
             "FROM seat_allocation a "
             "JOIN rooms r ON a.room_id = r.id "
             "JOIN students s ON a.student_id = s.id "
             "JOIN subjects sub ON a.subject_id = sub.id "
             "ORDER BY a.day, r.room_number, a.bench_number, a.seat_number");

    if (mysql_query(conn, queryStr)) {
        fprintf(stderr, "Query failed: %s\n", mysql_error(conn));
        fclose(file);
        return;
    }

    res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "Could not retrieve data: %s\n", mysql_error(conn));
        fclose(file);
        return;
    }

    if (mysql_num_rows(res) == 0) {
        fprintf(file, "No seat allocations available.\n");
        printf("Query returned no results.\n");
        mysql_free_result(res);
        fclose(file);
        return;
    }

    // Variables to track current day and room to create sections
    int current_day = -1, current_room = -1;
    int two_seater_count = 0, three_seater_count = 0;
    int current_bench = -1;

    fprintf(file, "Allocated Seats Matrix\n");
    fprintf(file, "=======================\n\n");

    while ((row = mysql_fetch_row(res))) {
        int day = atoi(row[0]);
        int room_number = atoi(row[1]);
        int bench_number = atoi(row[2]);
        int seat_number = atoi(row[5]);
        const char *symbol_number = row[6];
        const char *subject_name = row[7];

        // Section for a new day
        if (day != current_day) {
            if (current_day != -1) fprintf(file, "\n\n\n"); // Add spacing between days
            fprintf(file, "Day %d\n", day);
            fprintf(file, "-------\n");
            current_day = day;
            current_room = -1; // Reset room tracking
        }

        // Section for a new room
        if (room_number != current_room) {
            if (current_room != -1) fprintf(file, "\n\n"); // Add spacing between rooms
            fprintf(file, "Room %d\n", room_number);
            fprintf(file, "-------\n");
            current_room = room_number;

            // Extract bench capacities for the room
            two_seater_count = atoi(row[3]);
            three_seater_count = atoi(row[4]);
        }

        // Section for a new bench
        if (bench_number != current_bench) {
            if (current_bench != -1) fprintf(file, "\n"); // Add spacing between benches
            fprintf(file, "Bench %d: ", bench_number);
            current_bench = bench_number;
        }

        // Display seat allocation
        fprintf(file, "%s (%s), ", symbol_number, subject_name);

        // Add spacing and pipe based on seater type
        if ((seat_number == 2 && bench_number <= two_seater_count) ||
            (seat_number == 3 && bench_number > two_seater_count)) {
            fprintf(file, "");  // End of row for 2- or 3-seater bench
        }
    }

    fprintf(file, "\n\nMatrix export complete.\n");

    mysql_free_result(res);
    fclose(file);
    printf("\nSeat matrix exported successfully to %s.\n", filename);
}


void getPassword(char *password, size_t size) {
    printf("Enter password: ");
    fflush(stdout);

    char ch;
    size_t i = 0;

    while (i < size - 1 && (ch = getch()) != '\r') {
        if (ch == '\b') { // Handle backspace
            if (i > 0) {
                printf("\b \b");
                i--;
            }
        } else {
            password[i++] = ch;
            printf("*"); // Show asterisks for characters entered
        }
    }

    password[i] = '\0'; // Null-terminate the password
    printf("\n");
}

int registerUser() {
    char username[50], password[50];
    int role;

    printf("\nEnter username: ");
    scanf("%s", username);

    // Check if the username already exists
    char query[256];
    snprintf(query, sizeof(query), "SELECT id FROM users WHERE username='%s'", username);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "Query failed: %s\n", mysql_error(conn));
        return EXIT_FAILURE;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "mysql_store_result() failed: %s\n", mysql_error(conn));
        return EXIT_FAILURE;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row != NULL) {
        printf("Username already exists. Please choose another.\n");
        mysql_free_result(res);
        return EXIT_FAILURE;
    }
    mysql_free_result(res);

    // Securely get the password
    getPassword(password, sizeof(password));

    printf("Enter role (1 for Admin, 2 for Exam Coordinator): ");
    scanf("%d", &role);

    // Insert new user with hashed password into the users table
    snprintf(query, sizeof(query),
             "INSERT INTO users (username, password, role) VALUES ('%s', MD5('%s'), %d)",
             username, password, role);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "Error registering user: %s\n", mysql_error(conn));
        return EXIT_FAILURE;
    }

    printf("\nUser registered successfully.\n");
    return EXIT_SUCCESS;
}


int login(char *username, int *role) {
    char password[50];
    char query[256];

    // Securely get the password
    getPassword(password, sizeof(password));

    // Prepare the query with hashed password
    snprintf(query, sizeof(query), 
             "SELECT role FROM users WHERE username='%s' AND password=MD5('%s')", 
             username, password);

    // Execute the query
    if (mysql_query(conn, query)) {
        fprintf(stderr, "Query failed: %s\n", mysql_error(conn));
        return EXIT_FAILURE;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "Could not retrieve result: %s\n", mysql_error(conn));
        return EXIT_FAILURE;
    }

    // Check if the user exists and password matches
    MYSQL_ROW row;
    if ((row = mysql_fetch_row(res)) == NULL) {
        printf("Invalid username or password.\n");
        mysql_free_result(res);
        return EXIT_FAILURE;
    }

    // Fetch role
    *role = atoi(row[0]);

    // Clean up
    mysql_free_result(res);

    return EXIT_SUCCESS;
}