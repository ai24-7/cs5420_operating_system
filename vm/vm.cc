#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <cstring>
#include <climits>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

using namespace std;

// Structure representing a page table entry
struct Page {
    int frameNumber = -1;
    int isOnDisk = 0;
    int backingStoreBlock = -1;
    string status = "UNUSED";
};

// Structure representing a frame table entry
struct Frame {
    int first_use = -1;
    int isInUse = 0;
    int isDirty = 0;
    int last_use = -1;
    int pageNumber = -1;
};

// Structure representing a backing store block
struct BackingStoreBlock {
    int writeCount = 0;
    int isInUse = 0;
    int pageNumber = -1;
    int readCount = 0;
};

// Global variables
char *inputFilename = nullptr;
char *replacementAlgorithm = nullptr;
char *programName;

static int debugMode = 0;
static int totalBackingStoreBlocks = 0;
bool backingStoreEnabled = false;

size_t totalPages = 0, totalFrames = 0, pageSize = 0;
int totalPageReferences = 0, totalPagesMapped = 0, totalPageMisses = 0;
int totalFramesStolen = 0, totalFramesWrittenToDisk = 0, totalFramesRecoveredFromDisk = 0;
map<int, char> pageOperationMap;

string initialConfigLine;
bool isInitialConfigPrinted = false;

BackingStoreBlock *backingStoreTable = nullptr;
int backingStoreBlocksInUse = 0;
int backingStoreBlocksRead = 0;
int backingStoreBlocksWritten = 0;

vector<string> inputLines;

// For OPTIMAL algorithm optimization
unordered_map<int, vector<int>> futurePageReferences;

// Function declarations
void DisplayResults(Page *pageTable, Frame *frameTable, bool isFinalReport = false);
int SelectFrameForReplacement(Frame *frameTable);
int FindAvailableFrame(Frame *frameTable);
static void ShowUsage();
int FindAvailableBackingStoreBlock();
void ProcessInputLine(string line, size_t lineNumber, Page *pageTable, Frame *frameTable);
void InitializeBackingStore();
void AnalyzeFuturePageReferences();
void DisplayInitialConfiguration();
void ExecutePageReplacement(int currentPage, int &selectedFrame, Page *pageTable, Frame *frameTable);
void UpdateFrameAndPageEntries(int currentPage, int selectedFrame, char operation, Page *pageTable, Frame *frameTable, bool isCacheHit);
void HandlePageLoadingFromDisk(int currentPage, bool isCacheHit, Page *pageTable);
void ParseCommandLineArguments(int argc, char *argv[]);
void LoadInputFile();
void ProcessAllInputLines(Page *pageTable, Frame *frameTable);
void ReleaseResources(Page *pageTable, Frame *frameTable);

// Main function
int main(int argc, char *argv[]) {
    programName = argv[0];

    ParseCommandLineArguments(argc, argv);

    LoadInputFile();

    InitializeBackingStore();

    Page *pageTable = new Page[totalPages];
    Frame *frameTable = new Frame[totalFrames];

    AnalyzeFuturePageReferences();

    DisplayInitialConfiguration();

    ProcessAllInputLines(pageTable, frameTable);

    // Print final results
    DisplayResults(pageTable, frameTable, true);

    ReleaseResources(pageTable, frameTable);

    return 0;
}

void InitializeBackingStore() {
    // Initialize backing store if enabled
    if (backingStoreEnabled && totalBackingStoreBlocks > 0) {
        backingStoreTable = new BackingStoreBlock[totalBackingStoreBlocks];
    }
}

void AnalyzeFuturePageReferences() {
    // Preprocess future page references for OPTIMAL algorithm
    if (strcmp(replacementAlgorithm, "OPTIMAL") == 0) {
        unordered_map<int, vector<int>> pageUses;
        for (size_t lineIndex = 0; lineIndex < inputLines.size(); lineIndex++) {
            string line = inputLines[lineIndex];

            // Skip comments and empty lines
            if (line.empty() || line[0] == '#') continue;

            // Trim leading/trailing whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);

            // Skip commands
            if (line == "print" || line == "debug" || line == "nodebug") continue;

            // Parse operation and memory location
            istringstream iss(line);
            char operation;
            string memLocationStr;

            if (!(iss >> operation >> memLocationStr)) {
                continue; // Skip invalid lines
            }

            // Parse memory location (supporting hexadecimal without '0x' prefix)
            int memLocation;
            try {
                memLocation = stoi(memLocationStr, nullptr, 16); // Base 16 for hexadecimal
            } catch (...) {
                continue;
            }

            // Calculate page number
            int pageNum = (memLocation / pageSize) % totalPages;
            pageUses[pageNum].push_back(lineIndex);
        }
        futurePageReferences = pageUses;
    }
}

void DisplayInitialConfiguration() {
    // Print initial configuration
    if (!isInitialConfigPrinted && debugMode == 0) {
        isInitialConfigPrinted = true;

        // Output remains unchanged
        cout << "Page size: " << pageSize << endl;
        cout << "Num frames: " << totalFrames << endl;
        cout << "Num pages: " << totalPages << endl;
        cout << "Num backing blocks: " << totalBackingStoreBlocks << endl;
        cout << "Reclaim algorithm: " << replacementAlgorithm << endl;
    }
}

void ParseCommandLineArguments(int argc, char *argv[]) {
    if (argc < 3) {  // Need at least program name + algo + filename
        ShowUsage();
    }

    bool algorithmSpecified = false;

    for (int i = 1; i < argc; i++) {
        char* arg = argv[i];

        // Handle flags
        if (arg[0] == '-') {
            // Validate flag format (single character)
            if (!arg[1] || arg[2]) {
                ShowUsage();
            }

            if (arg[1] == 'd') {
                debugMode = 1;
            }
            else if (arg[1] == 'w') {
                backingStoreEnabled = true;
            }
            else {
                ShowUsage();
            }
            continue;
        }

        // Use array lookup for algorithm validation
        static const struct { const char* name; bool* found; } VALID_ALGORITHMS[] = {
            {"FIFO", &algorithmSpecified},
            {"LRU", &algorithmSpecified},
            {"OPTIMAL", &algorithmSpecified}
        };

        // Try to match algorithm first
        if (!algorithmSpecified) {
            bool matched = false;
            for (const auto& valid : VALID_ALGORITHMS) {
                if (!strcmp(arg, valid.name)) {
                    replacementAlgorithm = arg;
                    *valid.found = true;
                    matched = true;
                    break;
                }
            }
            if (matched) {
                continue;
            }
        }

        // If not algorithm and no filename yet, treat as filename
        if (!inputFilename) {
            inputFilename = arg;
        } else {
            ShowUsage();
        }
    }

    // Verify we got required parameters
    if (!algorithmSpecified || !inputFilename) {
        ShowUsage();
    }
}

void LoadInputFile() {
    ifstream inputFile(inputFilename);
    if (!inputFile.is_open()) {
        cerr << "Error: Cannot open file " << inputFilename << endl;
        exit(1);
    }

    // Read the first line (skip comments)
    string line;
    while (getline(inputFile, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;

        initialConfigLine = line;

        istringstream iss(line);
        // Read pageSize, numFrame, numPage, numBackingStoreBlocks
        if (!(iss >> pageSize >> totalFrames >> totalPages >> totalBackingStoreBlocks)) {
            cerr << "Error: Invalid format in the first line." << endl;
            exit(1);
        }
        break;
    }

    // Check if the necessary variables are set
    if (pageSize == 0 || totalFrames == 0 || totalPages == 0) {
        cerr << "Error: Missing or invalid page size, number of frames, or number of pages." << endl;
        exit(1);
    }

    // Read the rest of the lines
    while (getline(inputFile, line)) {
        inputLines.push_back(line);
    }

    inputFile.close();
}

void ProcessAllInputLines(Page *pageTable, Frame *frameTable) {
    // Process each line
    for (size_t lineIndex = 0; lineIndex < inputLines.size(); lineIndex++) {
        ProcessInputLine(inputLines[lineIndex], lineIndex, pageTable, frameTable);
    }
}

void ReleaseResources(Page *pageTable, Frame *frameTable) {
    // Clean up dynamically allocated memory
    delete[] pageTable;
    delete[] frameTable;
    if (backingStoreTable != nullptr) {
        delete[] backingStoreTable;
    }
}

void ExecutePageReplacement(int currentPage, int &selectedFrame, Page *pageTable, Frame *frameTable) {
    // Select a frame to replace using the replacement algorithm
    selectedFrame = SelectFrameForReplacement(frameTable);

    // Handle the page being replaced
    for (size_t i = 0; i < totalPages; i++) {
        if (pageTable[i].frameNumber == selectedFrame) {
            pageTable[i].status = "STOLEN";
            pageTable[i].frameNumber = -1;

            if (pageOperationMap.count(i) > 0) {
                if (pageOperationMap[i] == 'w' || frameTable[selectedFrame].isDirty == 1) {
                    pageTable[i].isOnDisk = 1;
                    totalFramesWrittenToDisk++;

                    if (backingStoreEnabled) {
                        // If the page does not already have a backing store block
                        if (pageTable[i].backingStoreBlock == -1) {
                            int bsIndex = FindAvailableBackingStoreBlock();
                            if (bsIndex == -1) {
                                cerr << "Error: No free backing store blocks available." << endl;
                                exit(1);
                            }
                            backingStoreTable[bsIndex].isInUse = 1;
                            backingStoreTable[bsIndex].pageNumber = i;
                            backingStoreTable[bsIndex].writeCount++;
                            backingStoreBlocksInUse++;
                            backingStoreBlocksWritten++;
                            pageTable[i].backingStoreBlock = bsIndex;
                        } else {
                            // Page already has a backing store block, increment writes
                            int bsIndex = pageTable[i].backingStoreBlock;
                            backingStoreTable[bsIndex].writeCount++;
                            backingStoreBlocksWritten++;
                        }
                    }
                }
            }

            totalFramesStolen++;
            break;
        }
    }
    frameTable[selectedFrame].first_use = totalPageReferences;
}

int SelectFrameForReplacement(Frame* frameTable) {
    if (strcmp(replacementAlgorithm, "FIFO") == 0) {
        int oldestFrame = 0;
        int earliestUse = frameTable[0].first_use;
        for (size_t i = 1; i < totalFrames; i++) {
            if (frameTable[i].first_use < earliestUse) {
                earliestUse = frameTable[i].first_use;
                oldestFrame = i;
            }
        }
        return oldestFrame;
    }

    if (strcmp(replacementAlgorithm, "LRU") == 0) {
        int lruFrame = 0;
        int leastRecentUse = frameTable[0].last_use;
        for (size_t i = 1; i < totalFrames; i++) {
            if (frameTable[i].last_use < leastRecentUse) {
                leastRecentUse = frameTable[i].last_use;
                lruFrame = i;
            }
        }
        return lruFrame;
    }
    // OPTIMAL
    int optimalFrame = 0;
    int farthestDistance = -1;
    for (size_t i = 0; i < totalFrames; i++) {
        int currentPage = frameTable[i].pageNumber;
        int distance;

        if (futurePageReferences[currentPage].empty()) {
            distance = INT_MAX;
        } else {
            distance = futurePageReferences[currentPage][0];
        }

        if (distance > farthestDistance) {
            farthestDistance = distance;
            optimalFrame = i;
        }
    }
    return optimalFrame;
}

// Function to update frame and page tables after a reference
void UpdateFrameAndPageEntries(int currentPage, int selectedFrame, char operation, Page *pageTable, Frame *frameTable, bool isCacheHit) {
    // Update the 'isDirty' flag based on cache hit and operation type
    if (operation == 'w' || (isCacheHit && frameTable[selectedFrame].isDirty == 1)) {
        frameTable[selectedFrame].isDirty = 1;
    } else {
        frameTable[selectedFrame].isDirty = 0;
    }

    // Update the frame number for the current page
    pageTable[currentPage].frameNumber = selectedFrame;

    // Mark the frame as in use
    frameTable[selectedFrame].isInUse = 1;

    // Update the 'first_use' timestamp if it hasn't been set yet
    if (frameTable[selectedFrame].first_use == -1) {
        frameTable[selectedFrame].first_use = totalPageReferences;
    } else {
        frameTable[selectedFrame].first_use = frameTable[selectedFrame].first_use;
    }

    // Update the 'last_use' timestamp to the current reference count
    frameTable[selectedFrame].last_use = totalPageReferences;

    // Associate the frame with the current page
    frameTable[selectedFrame].pageNumber = currentPage;
}

void HandlePageLoadingFromDisk(int currentPage, bool isCacheHit, Page *pageTable) {
    if (!isCacheHit && pageTable[currentPage].isOnDisk == 1) {
        totalFramesRecoveredFromDisk++;
        if (backingStoreEnabled && pageTable[currentPage].backingStoreBlock != -1) {
            int bsIndex = pageTable[currentPage].backingStoreBlock;
            backingStoreTable[bsIndex].readCount++;
            backingStoreBlocksRead++;
        }
    }
}

void ProcessInputLine(string line, size_t lineNumber, Page *pageTable, Frame *frameTable) {
    // Skip comments and empty lines
    if (line.empty() || line[0] == '#') return;

    // Trim leading/trailing whitespace
    line.erase(0, line.find_first_not_of(" \t\r\n"));
    line.erase(line.find_last_not_of(" \t\r\n") + 1);

    // Handle commands
    if (line == "print") {
        DisplayResults(pageTable, frameTable);
        return;
    }

    if (line == "debug") {
        debugMode = 1;
        return;
    }

    if (line == "nodebug") {
        debugMode = 0;
        return;
    }

    totalPageReferences++;

    // Parse operation and memory location
    istringstream iss(line);
    char operation;
    string memLocationStr;

    if (!(iss >> operation >> memLocationStr)) {
        cerr << "Error: Invalid line format at line " << lineNumber + 1 << ": " << line << endl;
        return; // Skip invalid lines
    }

    // Validate operation character
    if (operation != 'r' && operation != 'w') {
        cerr << "Error: Invalid operation '" << operation << "' at line " << lineNumber + 1 << endl;
        return;
    }

    // Parse memory location (supporting hexadecimal without '0x' prefix)
    int memLocation;
    try {
        memLocation = stoi(memLocationStr, nullptr, 16); // Base 16 for hexadecimal
    } catch (const invalid_argument &) {
        cerr << "Error: Invalid memory location at line " << lineNumber + 1 << ": " << memLocationStr << endl;
        return;
    } catch (const out_of_range &) {
        cerr << "Error: Memory location out of range at line " << lineNumber + 1 << ": " << memLocationStr << endl;
        return;
    }

    // Calculate page number
    int currentPage = (memLocation / pageSize) % totalPages;
    pageTable[currentPage].status = "MAPPED";

    int selectedFrame = -1;
    bool isCacheHit = false;

    // Update future uses for OPTIMAL algorithm
    if (strcmp(replacementAlgorithm, "OPTIMAL") == 0) {
        if (!futurePageReferences[currentPage].empty()) {
            futurePageReferences[currentPage].erase(futurePageReferences[currentPage].begin());
        }
    }

    // Check if page is already in a frame
    for (size_t i = 0; i < totalFrames; i++) {
        if (frameTable[i].pageNumber == currentPage) {
            isCacheHit = true;
            frameTable[i].last_use = totalPageReferences;
            selectedFrame = i;
            break;
        }
    }

    if (!isCacheHit) {
        totalPageMisses++;
        selectedFrame = FindAvailableFrame(frameTable);
    }

    // Update page operation map
    if (pageOperationMap.find(currentPage) == pageOperationMap.end()) {
        pageOperationMap[currentPage] = operation;
        totalPagesMapped++;
    } else {
        pageOperationMap[currentPage] = operation;
    }

    // If no empty frame is available, apply page replacement algorithm
    if (selectedFrame == -1) {
        ExecutePageReplacement(currentPage, selectedFrame, pageTable, frameTable);
    }

    // Update frame and page tables
    UpdateFrameAndPageEntries(currentPage, selectedFrame, operation, pageTable, frameTable, isCacheHit);

    // Handle loading page from disk
    HandlePageLoadingFromDisk(currentPage, isCacheHit, pageTable);
}

// Function to find any available (empty) frame
int FindAvailableFrame(Frame *frameTable) {
    for (size_t i = 0; i < totalFrames; i++) {
        if (frameTable[i].isInUse == 0)
            return i;
    }
    return -1;
}

// Function to find a free backing store block
int FindAvailableBackingStoreBlock() {
    for (int i = 0; i < totalBackingStoreBlocks; ++i) {
        if (backingStoreTable[i].isInUse == 0) {
            return i;
        }
    }
    return -1; // No free block
}

void DisplayResults(Page *pageTable, Frame *frameTable, bool isFinalReport) {
    if (debugMode || isFinalReport) {
        cout << "Page Table" << endl;
        for (size_t i = 0; i < totalPages; i++) {
            cout << setw(5) << i;
            if (pageTable[i].status == "UNUSED") {
                cout << " type:" << pageTable[i].status << endl;
            } else {
                cout << " type:" << pageTable[i].status
                     << " framenum:" << pageTable[i].frameNumber
                     << " ondisk:" << pageTable[i].isOnDisk;

                if (backingStoreEnabled && pageTable[i].backingStoreBlock != -1) {
                    cout << " bsblock:" << pageTable[i].backingStoreBlock;
                }
                cout << endl;
            }
        }

        cout << "Frame Table" << endl;
        for (size_t i = 0; i < totalFrames; i++) {
            cout << setw(5) << i;
            if (frameTable[i].isInUse == 0) {
                cout << " inuse:" << frameTable[i].isInUse << endl;
            } else {
                cout << " inuse:" << frameTable[i].isInUse
                     << " dirty:" << frameTable[i].isDirty
                     << " first_use:" << frameTable[i].first_use
                     << " last_use:" << frameTable[i].last_use << endl;
            }
        }
    }

    if (backingStoreEnabled) {
        cout << "Backing Store Table" << endl;
        for (int i = 0; i < totalBackingStoreBlocks; i++) {
            cout << setw(5) << i;
            if (backingStoreTable[i].isInUse == 0) {
                cout << " inuse:" << backingStoreTable[i].isInUse << endl;
            } else {
                cout << " inuse:" << backingStoreTable[i].isInUse
                     << " page:" << backingStoreTable[i].pageNumber
                     << " reads:" << backingStoreTable[i].readCount
                     << " writes:" << backingStoreTable[i].writeCount << endl;
            }
        }
        cout << "  TTL BS blocks inuse: " << backingStoreBlocksInUse << endl
             << "  TTL BS blocks read: " << backingStoreBlocksRead << endl
             << "  TTL BS blocks written: " << backingStoreBlocksWritten << endl;
    }

    cout << "Pages referenced: " << totalPageReferences << endl
         << "Pages mapped: " << totalPagesMapped << endl
         << "Page miss instances: " << totalPageMisses << endl
         << "Frame stolen instances: " << totalFramesStolen << endl
         << "Stolen frames written to swapspace: " << totalFramesWrittenToDisk << endl
         << "Stolen frames recovered from swapspace: " << totalFramesRecoveredFromDisk << endl;
}

// Function to display usage information
static void ShowUsage() {
    printf("usage: %s [-d] [-w] {FIFO|LRU|OPTIMAL} filename\n", programName);
    exit(1);
}
