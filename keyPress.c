#include "keyPress.h"
#include "oscUtility.h"  // For isMessagePrintingEnabled()
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <fcntl.h>

int keyPressSystemInitialized = 0;
int keyPressDebugEnabled = 0;
static int keypress_uinput_fd = -1;
KeyHashTable* keyHashTable = NULL;

// Key name to keycode mapping data
typedef struct {
    const char* name;
    int keycode;
    const char* description;
} KeyMappingData;

static const KeyMappingData keyMappingData[] = {
    // Letters
    {"a", KEY_A, "Letter A"}, {"b", KEY_B, "Letter B"}, {"c", KEY_C, "Letter C"},
    {"d", KEY_D, "Letter D"}, {"e", KEY_E, "Letter E"}, {"f", KEY_F, "Letter F"},
    {"g", KEY_G, "Letter G"}, {"h", KEY_H, "Letter H"}, {"i", KEY_I, "Letter I"},
    {"j", KEY_J, "Letter J"}, {"k", KEY_K, "Letter K"}, {"l", KEY_L, "Letter L"},
    {"m", KEY_M, "Letter M"}, {"n", KEY_N, "Letter N"}, {"o", KEY_O, "Letter O"},
    {"p", KEY_P, "Letter P"}, {"q", KEY_Q, "Letter Q"}, {"r", KEY_R, "Letter R"},
    {"s", KEY_S, "Letter S"}, {"t", KEY_T, "Letter T"}, {"u", KEY_U, "Letter U"},
    {"v", KEY_V, "Letter V"}, {"w", KEY_W, "Letter W"}, {"x", KEY_X, "Letter X"},
    {"y", KEY_Y, "Letter Y"}, {"z", KEY_Z, "Letter Z"},
    
    // Numbers
    {"0", KEY_0, "Number 0"}, {"1", KEY_1, "Number 1"}, {"2", KEY_2, "Number 2"},
    {"3", KEY_3, "Number 3"}, {"4", KEY_4, "Number 4"}, {"5", KEY_5, "Number 5"},
    {"6", KEY_6, "Number 6"}, {"7", KEY_7, "Number 7"}, {"8", KEY_8, "Number 8"},
    {"9", KEY_9, "Number 9"},
    
    // Function keys
    {"f1", KEY_F1, "Function F1"}, {"f2", KEY_F2, "Function F2"}, {"f3", KEY_F3, "Function F3"},
    {"f4", KEY_F4, "Function F4"}, {"f5", KEY_F5, "Function F5"}, {"f6", KEY_F6, "Function F6"},
    {"f7", KEY_F7, "Function F7"}, {"f8", KEY_F8, "Function F8"}, {"f9", KEY_F9, "Function F9"},
    {"f10", KEY_F10, "Function F10"}, {"f11", KEY_F11, "Function F11"}, {"f12", KEY_F12, "Function F12"},
    
    // Special keys
    {"space", KEY_SPACE, "Space bar"}, {"enter", KEY_ENTER, "Enter key"}, {"return", KEY_ENTER, "Return key"},
    {"tab", KEY_TAB, "Tab key"}, {"escape", KEY_ESC, "Escape key"}, {"esc", KEY_ESC, "Escape key"},
    {"backspace", KEY_BACKSPACE, "Backspace"}, {"delete", KEY_DELETE, "Delete key"},
    {"insert", KEY_INSERT, "Insert key"}, {"home", KEY_HOME, "Home key"}, {"end", KEY_END, "End key"},
    {"pageup", KEY_PAGEUP, "Page Up"}, {"pagedown", KEY_PAGEDOWN, "Page Down"},
    {"up", KEY_UP, "Up arrow"}, {"down", KEY_DOWN, "Down arrow"},
    {"left", KEY_LEFT, "Left arrow"}, {"right", KEY_RIGHT, "Right arrow"},
    
    // Modifiers
    {"ctrl", KEY_LEFTCTRL, "Control key"}, {"control", KEY_LEFTCTRL, "Control key"},
    {"alt", KEY_LEFTALT, "Alt key"}, {"shift", KEY_LEFTSHIFT, "Shift key"},
    {"super", KEY_LEFTMETA, "Super/Windows key"}, {"win", KEY_LEFTMETA, "Windows key"},
    {"menu", KEY_MENU, "Menu key"}, {"printscreen", KEY_SYSRQ, "Print Screen"},
    
    // Common symbols
    {"minus", KEY_MINUS, "Minus/Hyphen"}, {"equal", KEY_EQUAL, "Equal sign"},
    {"leftbrace", KEY_LEFTBRACE, "Left bracket"}, {"rightbrace", KEY_RIGHTBRACE, "Right bracket"},
    {"semicolon", KEY_SEMICOLON, "Semicolon"}, {"apostrophe", KEY_APOSTROPHE, "Apostrophe"},
    {"grave", KEY_GRAVE, "Grave accent"}, {"backslash", KEY_BACKSLASH, "Backslash"},
    {"comma", KEY_COMMA, "Comma"}, {"dot", KEY_DOT, "Period"}, {"slash", KEY_SLASH, "Forward slash"},
    
    {NULL, 0, NULL}
};

// Case-insensitive string comparison function
int strcasecmp_portable(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        int c1 = tolower(*s1);
        int c2 = tolower(*s2);
        if (c1 != c2) {
            return c1 - c2;
        }
        s1++;
        s2++;
    }
    return tolower(*s1) - tolower(*s2);
}

// Hash function - simple but effective for key names
unsigned int hashFunction(const char* key) {
    if (!key) return 0;
    
    unsigned int hash = 5381;  // djb2 hash algorithm
    int c;
    
    while ((c = *key++)) {
        // Convert to lowercase for case-insensitive hashing
        c = tolower(c);
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    
    return hash % KEY_HASH_TABLE_SIZE;
}

int initKeyHashTable(void) {
    if (keyHashTable) {
        return 0; // Already initialized
    }
    
    keyHashTable = malloc(sizeof(KeyHashTable));
    if (!keyHashTable) {
        printf("Failed to allocate memory for key hash table\n");
        return -1;
    }
    
    // Initialize all buckets to NULL
    for (int i = 0; i < KEY_HASH_TABLE_SIZE; i++) {
        keyHashTable->buckets[i] = NULL;
    }
    keyHashTable->size = 0;
    
    // Populate hash table with key mappings
    for (int i = 0; keyMappingData[i].name != NULL; i++) {
        if (insertKeyMapping(keyMappingData[i].name, 
                           keyMappingData[i].keycode, 
                           keyMappingData[i].description) < 0) {
            printf("Warning: Failed to insert key mapping for '%s'\n", keyMappingData[i].name);
        }
    }
    
    if (keyPressDebugEnabled) {
        printf("Key hash table initialized with %d entries\n", keyHashTable->size);
        printHashTableStats();
    }
    
    return 0;
}

void destroyKeyHashTable(void) {
    if (!keyHashTable) return;
    
    // Free all nodes in all buckets
    for (int i = 0; i < KEY_HASH_TABLE_SIZE; i++) {
        KeyHashNode* current = keyHashTable->buckets[i];
        while (current) {
            KeyHashNode* next = current->next;
            free(current);
            current = next;
        }
    }
    
    free(keyHashTable);
    keyHashTable = NULL;
    
    if (keyPressDebugEnabled) {
        printf("Key hash table destroyed\n");
    }
}

int insertKeyMapping(const char* name, int keycode, const char* description) {
    if (!keyHashTable || !name) return -1;
    
    unsigned int index = hashFunction(name);
    
    // Check if key already exists
    KeyHashNode* current = keyHashTable->buckets[index];
    while (current) {
        if (strcasecmp_portable(current->name, name) == 0) {
            // Update existing entry
            current->keycode = keycode;
            current->description = description;
            return 0;
        }
        current = current->next;
    }
    
    // Create new node
    KeyHashNode* newNode = malloc(sizeof(KeyHashNode));
    if (!newNode) {
        printf("Failed to allocate memory for key hash node\n");
        return -1;
    }
    
    strncpy(newNode->name, name, MAX_KEY_NAME_LENGTH - 1);
    newNode->name[MAX_KEY_NAME_LENGTH - 1] = '\0';
    newNode->keycode = keycode;
    newNode->description = description;
    newNode->next = keyHashTable->buckets[index];
    
    keyHashTable->buckets[index] = newNode;
    keyHashTable->size++;
    
    return 0;
}

int getKeycodeFromName(const char* keyName) {
    if (!keyHashTable || !keyName) return -1;
    
    unsigned int index = hashFunction(keyName);
    KeyHashNode* current = keyHashTable->buckets[index];
    
    while (current) {
        if (strcasecmp_portable(current->name, keyName) == 0) {
            return current->keycode;
        }
        current = current->next;
    }
    
    return -1; // Key not found
}

const char* getKeyNameFromCode(int keycode) {
    if (!keyHashTable) return "unknown";
    
    // Linear search through all buckets (reverse lookup is less common)
    for (int i = 0; i < KEY_HASH_TABLE_SIZE; i++) {
        KeyHashNode* current = keyHashTable->buckets[i];
        while (current) {
            if (current->keycode == keycode) {
                return current->name;
            }
            current = current->next;
        }
    }
    
    return "unknown";
}

void printHashTableStats(void) {
    if (!keyHashTable) {
        printf("Hash table not initialized\n");
        return;
    }
    
    int usedBuckets = 0;
    int maxChainLength = 0;
    int totalChainLength = 0;
    
    for (int i = 0; i < KEY_HASH_TABLE_SIZE; i++) {
        if (keyHashTable->buckets[i]) {
            usedBuckets++;
            int chainLength = 0;
            KeyHashNode* current = keyHashTable->buckets[i];
            while (current) {
                chainLength++;
                current = current->next;
            }
            if (chainLength > maxChainLength) {
                maxChainLength = chainLength;
            }
            totalChainLength += chainLength;
        }
    }
    
    printf("=== Key Hash Table Statistics ===\n");
    printf("Total entries: %d\n", keyHashTable->size);
    printf("Table size: %d buckets\n", KEY_HASH_TABLE_SIZE);
    printf("Used buckets: %d (%.1f%%)\n", usedBuckets, 
           (float)usedBuckets / KEY_HASH_TABLE_SIZE * 100);
    printf("Load factor: %.3f\n", (float)keyHashTable->size / KEY_HASH_TABLE_SIZE);
    printf("Max chain length: %d\n", maxChainLength);
    printf("Average chain length: %.2f\n", 
           usedBuckets > 0 ? (float)totalChainLength / usedBuckets : 0.0);
}

int setupKeypressUinputDevice(void) {
    if (keypress_uinput_fd >= 0) {
        return keypress_uinput_fd; // Already initialized
    }
    
    keypress_uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (keypress_uinput_fd < 0) {
        perror("Failed to open /dev/uinput for keypress - you may need to run as root or add user to input group");
        return -1;
    }

    // Enable key events
    ioctl(keypress_uinput_fd, UI_SET_EVBIT, EV_KEY);
    ioctl(keypress_uinput_fd, UI_SET_EVBIT, EV_SYN);
    
    // Enable all keys we might want to send
    if (keyHashTable) {
        for (int i = 0; i < KEY_HASH_TABLE_SIZE; i++) {
            KeyHashNode* current = keyHashTable->buckets[i];
            while (current) {
                ioctl(keypress_uinput_fd, UI_SET_KEYBIT, current->keycode);
                current = current->next;
            }
        }
    }
    
    // Enable additional common keys
    for (int key = KEY_ESC; key <= KEY_MICMUTE; key++) {
        ioctl(keypress_uinput_fd, UI_SET_KEYBIT, key);
    }

    struct uinput_user_dev uidev;
    memset(&uidev, 0, sizeof(uidev));
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "OSC-KeyPress-Control");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor = 0x1234;
    uidev.id.product = 0x5679;
    uidev.id.version = 1;

    write(keypress_uinput_fd, &uidev, sizeof(uidev));
    ioctl(keypress_uinput_fd, UI_DEV_CREATE);

    if (keyPressDebugEnabled) {
        printf("KeyPress control device initialized\n");
    }

    return keypress_uinput_fd;
}

void cleanupKeypressUinputDevice(void) {
    if (keypress_uinput_fd >= 0) {
        ioctl(keypress_uinput_fd, UI_DEV_DESTROY);
        close(keypress_uinput_fd);
        keypress_uinput_fd = -1;
        if (keyPressDebugEnabled) {
            printf("KeyPress control device cleaned up\n");
        }
    }
}

int initKeyPressSystem(void) {
    if (keyPressSystemInitialized) {
        return 0; // Already initialized
    }
    
    // Initialize hash table first
    if (initKeyHashTable() < 0) {
        return -1;
    }
    
    if (setupKeypressUinputDevice() < 0) {
        destroyKeyHashTable();
        return -1;
    }
    
    keyPressSystemInitialized = 1;
    keyPressDebugEnabled = isMessagePrintingEnabled();
    
    if (keyPressDebugEnabled) {
        printf("KeyPress system initialized with hash table optimization\n");
    }
    
    return 0;
}

void shutdownKeyPressSystem(void) {
    if (keyPressSystemInitialized) {
        cleanupKeypressUinputDevice();
        destroyKeyHashTable();
        keyPressSystemInitialized = 0;
        if (keyPressDebugEnabled) {
            printf("KeyPress system shutdown\n");
        }
    }
}

int sendKeyEvent(int fd, int keycode, int value) {
    struct input_event ie;
    memset(&ie, 0, sizeof(ie));
    ie.type = EV_KEY;
    ie.code = keycode;
    ie.value = value;
    ie.time.tv_sec = 0;
    ie.time.tv_usec = 0;
    
    if (write(fd, &ie, sizeof(ie)) != sizeof(ie)) {
        return -1;
    }
    
    // Send sync event
    ie.type = EV_SYN;
    ie.code = SYN_REPORT;
    ie.value = 0;
    return write(fd, &ie, sizeof(ie));
}

int sendSingleKey(int keycode) {
    int fd = setupKeypressUinputDevice();
    if (fd < 0) return 0;
    
    // Press key
    if (sendKeyEvent(fd, keycode, 1) < 0) return 0;
    
    // Small delay - use sleep instead of usleep for C99 compatibility
    struct timespec ts = {0, 10000000}; // 10ms in nanoseconds
    nanosleep(&ts, NULL);
    
    // Release key
    if (sendKeyEvent(fd, keycode, 0) < 0) return 0;
    
    if (keyPressDebugEnabled) {
        printf("KeyPress: Sent key %s (%d)\n", getKeyNameFromCode(keycode), keycode);
    }
    
    return 1;
}

int sendKeyCombo(const KeyAction* keys, int keyCount) {
    int fd = setupKeypressUinputDevice();
    if (fd < 0 || !keys || keyCount <= 0) return 0;
    
    // Press all keys in order
    for (int i = 0; i < keyCount; i++) {
        if (sendKeyEvent(fd, keys[i].keycode, 1) < 0) return 0;
        struct timespec ts = {0, 5000000}; // 5ms
        nanosleep(&ts, NULL);
    }
    
    // Small delay while keys are held
    struct timespec ts = {0, 20000000}; // 20ms
    nanosleep(&ts, NULL);
    
    // Release all keys in reverse order
    for (int i = keyCount - 1; i >= 0; i--) {
        if (sendKeyEvent(fd, keys[i].keycode, 0) < 0) return 0;
        struct timespec ts2 = {0, 5000000}; // 5ms
        nanosleep(&ts2, NULL);
    }
    
    if (keyPressDebugEnabled) {
        printf("KeyPress: Sent key combo with %d keys\n", keyCount);
    }
    
    return 1;
}

int sendKeySequence(const KeyAction* keys, int keyCount) {
    if (!keys || keyCount <= 0) return 0;
    
    // Send each key individually with delays
    for (int i = 0; i < keyCount; i++) {
        if (!sendSingleKey(keys[i].keycode)) return 0;
        
        // Delay between keys in sequence
        if (i < keyCount - 1) {
            struct timespec ts = {0, 50000000}; // 50ms
            nanosleep(&ts, NULL);
        }
    }
    
    if (keyPressDebugEnabled) {
        printf("KeyPress: Sent key sequence with %d keys\n", keyCount);
    }
    
    return 1;
}

int sendKeyHold(int keycode, int duration_ms) {
    int fd = setupKeypressUinputDevice();
    if (fd < 0) return 0;
    
    // Press key
    if (sendKeyEvent(fd, keycode, 1) < 0) return 0;
    
    // Hold for specified duration
    struct timespec ts = {duration_ms / 1000, (duration_ms % 1000) * 1000000};
    nanosleep(&ts, NULL);
    
    // Release key
    if (sendKeyEvent(fd, keycode, 0) < 0) return 0;
    
    if (keyPressDebugEnabled) {
        printf("KeyPress: Held key %s for %dms\n", getKeyNameFromCode(keycode), duration_ms);
    }
    
    return 1;
}

int parseKeyString(const char* keyString, KeyPressAction* action) {
    if (!keyString || !action) return 0;
    
    memset(action, 0, sizeof(KeyPressAction));
    
    // Make a copy to work with
    char workString[256];
    strncpy(workString, keyString, sizeof(workString) - 1);
    workString[sizeof(workString) - 1] = '\0';
    
    // Check for special prefixes
    if (strncmp(workString, "hold:", 5) == 0) {
        action->type = KEY_ACTION_HOLD;
        // Parse hold:key:duration format
        char* keyPart = workString + 5;
        char* durationPart = strchr(keyPart, ':');
        if (durationPart) {
            *durationPart = '\0';
            durationPart++;
            action->keys[0].duration_ms = atoi(durationPart);
        } else {
            action->keys[0].duration_ms = 500; // Default 500ms
        }
        
        int keycode = getKeycodeFromName(keyPart);
        if (keycode < 0) return 0;
        
        action->keys[0].keycode = keycode;
        action->keyCount = 1;
        snprintf(action->description, sizeof(action->description), "Hold %.20s for %dms", 
                keyPart, action->keys[0].duration_ms);
        return 1;
    }
    
    // Check for sequence (keys separated by spaces)
    if (strchr(workString, ' ')) {
        action->type = KEY_ACTION_SEQUENCE;
        char* token = strtok(workString, " ");
        while (token && action->keyCount < 8) {
            int keycode = getKeycodeFromName(token);
            if (keycode < 0) return 0;
            
            action->keys[action->keyCount].keycode = keycode;
            action->keyCount++;
            token = strtok(NULL, " ");
        }
        snprintf(action->description, sizeof(action->description), "Key sequence with %d keys", action->keyCount);
        return 1;
    }
    
    // Check for combination (keys separated by +)
    if (strchr(workString, '+')) {
        action->type = KEY_ACTION_COMBO;
        char* token = strtok(workString, "+");
        while (token && action->keyCount < 8) {
            int keycode = getKeycodeFromName(token);
            if (keycode < 0) return 0;
            
            action->keys[action->keyCount].keycode = keycode;
            action->keyCount++;
            token = strtok(NULL, "+");
        }
        snprintf(action->description, sizeof(action->description), "Key combo with %d keys", action->keyCount);
        return 1;
    }
    
    // Single key
    action->type = KEY_ACTION_SINGLE;
    int keycode = getKeycodeFromName(workString);
    if (keycode < 0) return 0;
    
    action->keys[0].keycode = keycode;
    action->keyCount = 1;
    snprintf(action->description, sizeof(action->description), "Single key: %.20s", workString);
    return 1;
}

int executeKeyPress(const char* keyString) {
    if (!keyString) return 0;
    
    if (initKeyPressSystem() < 0) {
        printf("Failed to initialize keypress system\n");
        return 0;
    }
    
    KeyPressAction action;
    if (!parseKeyString(keyString, &action)) {
        printf("Failed to parse key string: %s\n", keyString);
        return 0;
    }
    
    if (keyPressDebugEnabled) {
        printf("Executing keypress: %s\n", action.description);
    }
    
    switch (action.type) {
        case KEY_ACTION_SINGLE:
            return sendSingleKey(action.keys[0].keycode);
            
        case KEY_ACTION_COMBO:
            return sendKeyCombo(action.keys, action.keyCount);
            
        case KEY_ACTION_SEQUENCE:
            return sendKeySequence(action.keys, action.keyCount);
            
        case KEY_ACTION_HOLD:
            return sendKeyHold(action.keys[0].keycode, action.keys[0].duration_ms);
            
        default:
            printf("Unknown key action type\n");
            return 0;
    }
}

void listAvailableKeys(void) {
    if (!keyHashTable) {
        printf("Key hash table not initialized\n");
        return;
    }
    
    printf("Available Keys for KeyPress Actions (Total: %d):\n\n", keyHashTable->size);
    
    printf("Letters: a-z\n");
    printf("Numbers: 0-9\n");
    printf("Function Keys: f1-f12\n");
    printf("Arrows: up, down, left, right\n");
    printf("Modifiers: ctrl, alt, shift, super/win\n");
    printf("Special: space, enter, tab, escape, backspace, delete\n");
    printf("Navigation: home, end, pageup, pagedown, insert\n");
    printf("Symbols: minus, equal, comma, dot, slash, etc.\n\n");
    
    printf("Full list of supported keys:\n");
    int count = 0;
    for (int i = 0; i < KEY_HASH_TABLE_SIZE; i++) {
        KeyHashNode* current = keyHashTable->buckets[i];
        while (current) {
            if (count % 6 == 0) printf("\n");
            printf("%-12s", current->name);
            count++;
            current = current->next;
        }
    }
    printf("\n\nHash table performance: O(1) average lookup time\n");
}

void listKeyPressExamples(void) {
    printf("KeyPress Action Examples:\n\n");
    
    printf("Single Keys:\n");
    printf("  @key:a                    - Press 'a' key\n");
    printf("  @key:f1                   - Press F1 key\n");
    printf("  @key:space                - Press space bar\n");
    printf("  @key:enter                - Press enter key\n\n");
    
    printf("Key Combinations (simultaneous):\n");
    printf("  @key:ctrl+c               - Ctrl+C (copy)\n");
    printf("  @key:ctrl+v               - Ctrl+V (paste)\n");
    printf("  @key:alt+tab              - Alt+Tab (switch windows)\n");
    printf("  @key:ctrl+shift+esc       - Ctrl+Shift+Esc (task manager)\n\n");
    
    printf("Key Sequences (one after another):\n");
    printf("  @key:ctrl+a ctrl+c        - Select all, then copy\n");
    printf("  @key:alt+f4               - Alt+F4 (close window)\n");
    printf("  @key:win+r                - Windows+R (run dialog)\n\n");
    
    printf("Hold Keys:\n");
    printf("  @key:hold:space:1000      - Hold space for 1000ms\n");
    printf("  @key:hold:w:500           - Hold 'w' for 500ms\n\n");
    
    printf("Usage in CLI:\n");
    printf("  action discordmute @key:ctrl+shift+m\n");
    printf("  action screenshot @key:printscreen\n");
    printf("  action volume-up @key:ctrl+up\n");
    
    printf("\nNote: Hash table provides O(1) key lookup performance!\n");
}

int executeBuiltinKeyAction(const char* actionName, const char* parameter) {
    if (strcmp(actionName, "key") == 0 && parameter) {
        return executeKeyPress(parameter);
    }
    
    // Predefined key actions
    if (strcmp(actionName, "copy") == 0) {
        return executeKeyPress("ctrl+c");
    } else if (strcmp(actionName, "paste") == 0) {
        return executeKeyPress("ctrl+v");
    } else if (strcmp(actionName, "cut") == 0) {
        return executeKeyPress("ctrl+x");
    } else if (strcmp(actionName, "undo") == 0) {
        return executeKeyPress("ctrl+z");
    } else if (strcmp(actionName, "redo") == 0) {
        return executeKeyPress("ctrl+y");
    } else if (strcmp(actionName, "select-all") == 0) {
        return executeKeyPress("ctrl+a");
    } else if (strcmp(actionName, "alt-tab") == 0) {
        return executeKeyPress("alt+tab");
    } else if (strcmp(actionName, "screenshot") == 0) {
        return executeKeyPress("printscreen");
    }
    
    return 0; // Unknown action
}
