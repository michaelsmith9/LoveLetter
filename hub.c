/*
 * The hub program
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

/*Error exit statuses*/
#define NORMAL_EXIT 0
#define USAGE_ERROR 1
#define ACCESS_ERROR 2
#define DECK_ERROR 3
#define FORK_ERROR 4
#define QUIT_ERROR 5
#define MESSAGE_ERROR 6
#define SIGINT_ERROR 7
/* Read and write defines */
#define READ 0
#define WRITE 1
/*A common shift for chars to ints and vice versa*/
#define SHIFT 65

/* A generic Deck struct to contain a list of 16 deck cards 
 * - cards: a list of 16 cards
 * - pos: the pos of the next card to get in the deck
 * - nextDeck: the next deck after this deck
 * */
typedef struct Deck {
    char cards[16]; 
    int pos; 
    struct Deck* nextDeck;
} Deck;

/* A stream struct, containing both a read and write file pointer
 * - write: the file pointer to write to for this stream
 * - read: the file pointer to read from for this stream
 */
typedef struct Stream {
    FILE* write; 
    FILE* read;
} Stream;

/* A player struct 
 * - outOfRound: boolean for out of round status
 * - label: the player label
 * - holding: what the player is holding
 * - protected: boolean for the protection status
 */
typedef struct Player {
    bool outOfRound;
    char label; 
    char holding; 
    bool protected; 
} Player;

/* A game struct containing all the information about this game
 * - round: the current round
 * - numPlayers: the number of players (child processes)
 * - deck: the game deck (for this round)
 * - scores: an array of scores for each player
 * - pipes: a list of streams for 2 way communication with children
 * - players: a list of players in the game
 */
typedef struct Game {
    int round;
    int numPlayers; 
    struct Deck* deck;
    int* scores; 
    struct Stream* pipes; 
    struct Player* players; 
} Game;

/* struct of child processes' PIDs
 * - pid: a list of process IDs
 * - numChildren: the number of child processes
 */
typedef struct ChildProcesses {
    int* pid; 
    int numChildren; 
} ChildProcesses;

/* Global variables */

// a struct to contain the PIDs of all children
struct ChildProcesses* children = NULL;

/*
 * Exits the process with the given exitStatus.
 */ 
void exit_with(int exitStatus) {
    switch(exitStatus) {
        case 0:
            exit(0);
            break;
        case 1:
            fprintf(stderr, "Usage: hub deckfile prog1 prog2 [prog3"
                    " [prog4]]\n");
            exit(1);
            break;
        case 2:
            fprintf(stderr, "Unable to access deckfile\n");
            exit(2);
            break;
        case 3:
            fprintf(stderr, "Error reading deck\n");
            exit(3);
            break;
        case 4:
            fprintf(stderr, "Unable to start subprocess\n");
            exit(4);
            break;
        case 5:
            fprintf(stderr, "Player quit\n");
            exit(5);
            break;
        case 6:
            fprintf(stderr, "Invalid message received from player\n");
            exit(6);
            break;
        case 7: 
            fprintf(stderr, "SIGINT caught\n");
            exit(7);
            break;
        default:
            break;
    }
}

/*
 * Checks the sum and type of chars in the input array - corresponding to
 * a line in a deck file. The sum of all chars in the array must be 54.
 * There must be 5 x '1', 2 x '2', 2 x '3', 2 x '4', 2 x '5', 1 x '6',
 * 1 x '7' and 1 x '8'. 
 * Triggers exit if input does not contain those chars (as listed above).
 */
void check_sum(char input[16]) {
    /*Variables to be used*/
    int getVal; //the value retrieved
    int sum = 0; //the sum of the deck
    int max[] = {5, 2, 2, 2, 2, 1, 1, 1}; //the maximum number of cards
    int val[16]; //an array to store the values
    int number[8] = {0, 0, 0, 0, 0, 0, 0, 0}; //the number of cards counted

    // add the input to the val array
    for (int i = 0; i < 16; i++) {
        getVal = input[i] - '0'; //shift from char to int
        val[i] = input[i] - '0';
        sum += getVal; //increment the sum
    }
    //check right number of cards
    if (sum != 54) {
        exit_with(DECK_ERROR);
    }
    for (int i = 0; i < 16; i++) {
        number[val[i] - 1] += 1;
    }
    //check if there are more of a certain card than allowed
    for (int i = 0; i < 8; i++) {
        if (number[i] != max[i]) {
            exit_with(DECK_ERROR);
        }
    }
}

/*
 * Creates a linked list from the given file deckFile. Exits process
 * upon error in opening deckFile, reading a line, or incorrect line of
 * file. 
 */
Deck *get_list_decks(FILE* deckFile) {
    //check if deckFile is NULL
    if (deckFile == NULL) {
        exit_with(ACCESS_ERROR);
    }
    char *in, input[18]; //get first line of deckfile and check length
    in = fgets(input, 18, deckFile);
    if (input[16] != '\n') {
        exit_with(DECK_ERROR);
    }
   
    //create the head of the deck linked list
    check_sum(input); //check the line has the right chars
    Deck *head = malloc(sizeof(Deck));
    head->pos = 1; //initialise to second card (i.e. discard 1st)
    for (int i = 0; i < 16; i++) {
        head->cards[i] = input[i]; //set head cards according to input
    }
    
    /* Make a linked list */
    head->nextDeck = NULL; //nextDeck is initially NULL
    in = fgets(input, 18, deckFile); //get 18 in incase incorrect line 
    while (in != NULL) { //loop through until we hit the last line
        check_sum(input); //check the sum of the line
        Deck* current = head;
        while (current->nextDeck != NULL) { //go until null entry
            current = current->nextDeck; //set current to nextDeck
        }
        Deck* newDeck = malloc(sizeof(Deck)); //malloc a newDeck
        newDeck->pos = 1;
        for (int j = 0; j < 16; j++) { //set up the newDeck
            newDeck->cards[j] = input[j];
        }
        newDeck->nextDeck = NULL; //the newDeck is the current deck's
        current->nextDeck = newDeck; //next deck
        in = fgets(input, 18, deckFile); //get more input
    }
    Deck* current = head; //current is the ead
    if (head->nextDeck == NULL) {
        //if only one Deck file, reuse it   
        head->nextDeck = head;
    } else {
        while(current->nextDeck != NULL) { //wrap the list around to make
            current = current->nextDeck; //it a repeating linked list
        }
        //next Deck is null, wrap around to head
        current->nextDeck = head;
    }
    return head;
}

/*
 * Handler for SIGINT. Sends SIGKILL to child processes in 
 * children global struct and performs a waitpid. Exits the program
 * with a SIGINT_ERROR
 */
void kill_children(int s) {
    int childStatus;
    for (int i = 0; i < children->numChildren; i++) {
        //kill child process and then wait on it
        kill(children->pid[i], SIGKILL);
        waitpid(children->pid[i], &childStatus, 0);
    }
    exit_with(SIGINT_ERROR);
}

/*
 * Performs a safe exit, shuts down child processes given in global
 * variable children by sending SIGKILL to them and waiting on them.
 * (NOTE: safe_exit as hub process reaps child processes instead
 * of leaving them to be reaped by init)
 */ 
void safe_exit(struct Game* game) {
    int childStatus;
    for (int i = 0; i < game->numPlayers; i++) {
        //send SIGKILL to child process
        kill(children->pid[i], SIGKILL);
        //wait for child process
        waitpid(children->pid[i], &childStatus, 0);
    }
}

/*
 * Initialise handlers for SIGINT and SIGPIPE
 */
void initialise_handler(void) {
    //initialise the SIGINT handler to kill child processes
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = kill_children;
    sigIntHandler.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sigIntHandler, 0);

    //initialise the SIGPIPE handler to ignore 
    //reading EOF from child will indicate a child has quit
    struct sigaction saPipeIgnore;
    saPipeIgnore.sa_handler = SIG_IGN;
    saPipeIgnore.sa_flags = SA_RESTART;
    sigaction(SIGPIPE, &saPipeIgnore, 0);
}

/*
 * Checks if fork was successful by reading first character from each
 * child process pipe in the struct game. If the character is a '-', then
 * the fork was successful, if not, the process exits.
 */
void check_successful_fork(struct Game* game) {
    char initialRead; //the char to read from the player
    for (int i = 0; i < game->numPlayers; i++) {
        //read from each pipe
        //fprintf(stderr, "here\n");
        initialRead = fgetc(game->pipes[i].read);
        //fprintf(stderr, "here2\n");
        if (initialRead != '-') {
            safe_exit(game);
            exit_with(FORK_ERROR);
        }
    }   
}

/* 
 * Attempts to create the processes given by childProgram. Will
 * exit program upon fork or pipe error. Sets up pipes in game to 
 * for bidirectional communication with child processes.
 */
void create_children(struct Game* game, char** childProgram) {
    /* Set up the bitBucket for stderr redirection */
    FILE* bitBucket = fopen("/dev/null", "w");
    int bitBucketNumber = fileno(bitBucket);
    char* count = malloc(sizeof(char) * 2); //create string for the count
    count[0] = game->numPlayers + 48; //make it ascii
    count[1] = '\0'; //add null terminator
    
    //Loop through once for each player and fork and exec the appropriate
    //process from childProgram
    for (int i = 0; i < game->numPlayers; i++) {
        int read[2]; //an array of file descriptorts for reading
        int write[2]; //an array of file descriptors for writing
        int pid; //the pid
    
        // pipe, and check for failure
        if(pipe(read) == -1 || pipe(write) == -1) {
            exit_with(FORK_ERROR);
        }
        pid = fork();
        if(pid) { //we are the parent (the hub)
            children->pid[i] = pid;
            game->pipes[i].write = fdopen(read[WRITE], "w");
            close(read[READ]); //close read end
            game->pipes[i].read = fdopen(write[READ], "r");
            close(write[WRITE]); //close write end    
            game->players[i].label = i + SHIFT; //shift for ascii
            game->players[i].outOfRound = false; //set not out
            game->players[i].protected = false; //set not protected
        } else {
            //we are the child
            close(read[WRITE]); //close write end
            dup2(read[READ], STDIN_FILENO); //redirect stdin to pipe
            close(read[READ]); //close read end
            close(write[READ]); //close read end
            dup2(write[WRITE], STDOUT_FILENO); //redirect stdout to pipe
            close(write[WRITE]); //close write end
            //hook up stderr to bitbucket
            dup2(bitBucketNumber, STDERR_FILENO); 
            char* designator = malloc(sizeof(char) * 2);
            designator[0] = i + SHIFT;
            designator[1] = '\0';   
            execlp(childProgram[i], "player", count, designator, NULL);
            fprintf(stdout, "F"); //failed if here, send F to indicate
            fflush(stdout); //failure
            exit(1); //exit
        }   
    }
    //check for successful fork
    check_successful_fork(game);
}

/*
 * Returns a boolean to indicate if there is a winner.
 * A winner is indicated by a player with a score == 4.
 */
bool is_winner(struct Game* game) {
    /* Initialise the return value */
    bool returnValue = false;
   
    //Loop through all players and check their score, check for 
    //>= 4 in case of error 
    for (int i = 0; i < game->numPlayers; i++) {
        if(game->scores[i] >= 4) {
            returnValue = true;
        }
    }
    return returnValue;
}

/*
 * Checks if end of round conditions are met. Conditions: all players bar 
 * one are eliminated or there are no cards left in the deck.
 */
bool check_end_of_round(struct Game* game) {
    //An int to record the number of players that are out
    int numOut = 0;
    
    //Loop through and check the number that are out
    for (int i = 0; i < game->numPlayers; i++) {
        if (game->players[i].outOfRound) {
            numOut++;
        }
    }
    //if it's 16 then we finished the Deck, and the round is over
    if(game->deck->pos == 16) {
        return true;
    } else if (numOut == (game->numPlayers - 1)) {
        //If all bar one players are out, then the round is over
        return true;
    } else {
        return false;
    }
}

/*
 * Sends scores to child processes and Round winner(s) message to stdout
 */ 
void send_scores(struct Game* game) {
    //Initialise the score string to contain 'scores' and the number 
    //of scores for the number of players playing
    char scoresString[8 + (2 * game->numPlayers)];
    
    //Build the string according to the number of players
    switch(game->numPlayers) {
        case 2: 
            sprintf(scoresString, "scores %d %d\n", game->scores[0],
                    game->scores[1]); 
            break;
        case 3: 
            sprintf(scoresString, "scores %d %d %d\n", game->scores[0],
                    game->scores[1], game->scores[2]); 
            break;
        case 4: 
            sprintf(scoresString, "scores %d %d %d %d\n", game->scores[0], 
                    game->scores[1], game->scores[2], game->scores[3]); 
            break;
        default: 
            sprintf(scoresString, "error\n"); 
            break;
    }
    //Send out the string
    for (int i = 0; i < game->numPlayers; i++) {
        fprintf(game->pipes[i].write, "%s", scoresString);
    }
    char high = '0';
    //Send out the round winner(s) message
    for (int j = 0; j < game->numPlayers; j++) {
        if (!(game->players[j].outOfRound) && 
                game->players[j].holding >= high) {
            high = game->players[j].holding;
        }
    }
    //Send round winners to stdout, adding " %c" (c is winner) accordingly
    fprintf(stdout, "Round winner(s) holding %c:", high);
    for (int k = 0; k < game->numPlayers; k++) {
        if(!(game->players[k].outOfRound) && game->players[k].holding
                == high) {
            fprintf(stdout, " %c", k + SHIFT);
        }
    }
    fprintf(stdout, "\n"); //top off with newline
    fflush(stdout);
}

/*
 * Returns the next card in the current deck, or '-' if the last card in
 * the deck has been reached.
 */
char get_next_card(struct Game* game) {
    //The current pos we are up to
    int pos = game->deck->pos;
    
    if (pos < 16) {
        pos = game->deck->pos++;
        return game->deck->cards[pos];
    } else {
        return '-';
    }
} 

/*
 * Checks if there are unprotected players who are still playing the game
 * and who are not the player.
 * Returns true if there are, false otherwise.
 */ 
bool target_available(struct Game* game, int player) {
    bool val = false; //the return value

    //check if they are unprotected and not out of the round and not
    //the player that initialised the search
    for (int i = 0; i < game->numPlayers; i++) {
        if (game->players[i].protected == false && 
                game->players[i].outOfRound == false && i != player) {
            val = true;
        }
    }
    return val;
}

/*
 * Sends a thishappened to each player process of the form:
 * - thishappened: player|card|targetPlayer|guessCard|/|dropper|dropped|
 *   elimated|\n
 * where:
 * - player: player who made move
 * - card: played card
 * - targetPlayer: the targetPlayer
 * - guessCard: the card being guessed
 * - dropper: the player dropping their card
 * - dropped: the dropped card
 * - eliminated: the eliminated player
 */ 
void send_this_happened(struct Game* game, char player, char card, 
        char targetPlayer, char guessCard, char dropper, char dropped,
        char eliminated) {
    //Send thishappened according to the given inputs
    for (int i = 0; i < game->numPlayers; i++) {
        fprintf(game->pipes[i].write, "thishappened %c%c%c%c/%c%c%c\n",
                player, card, targetPlayer, guessCard, dropper, dropped,
                eliminated);
        fflush(game->pipes[i].write);
    }  
}

/*
 * Perform a swap between a player and a targetPlayer. Triggered by a 6.
 * Where:
 * - player: player making move
 * - targetPlayer: the targeted player
 * - holding: the card the player is holding
 * - guess: the card they are guessing
 * If the player is holding a 7 then the process exits with MESSAGE_ERROR
 */ 
void swap(struct Game* game, int player, char targetPlayer, char holding, 
        char guess) {
    // can't discard 5/6 in preference to 7
    if (holding == '7' || (targetPlayer == '-' && 
            target_available(game, player)) || guess != '-') {
        safe_exit(game);
        exit_with(MESSAGE_ERROR);
    }
    
    //Check targetPlayer is a player
    if (targetPlayer != '-') {
        //send whatever the player was holding to target
        fprintf(game->pipes[targetPlayer - SHIFT].write, "replace %c\n", 
                holding);
        fflush(game->pipes[targetPlayer - SHIFT].write);
        
       //send what the target was holding to player
        fprintf(game->pipes[player].write, "replace %c\n", 
                game->players[targetPlayer - SHIFT].holding);
        fflush(game->pipes[player].write);
        
        //update state
        char c = game->players[targetPlayer - SHIFT].holding;
        game->players[targetPlayer - SHIFT].holding = 
                game->players[player].holding;
        game->players[player].holding = c;
        
        //print message to stdout
        fprintf(stdout, "Player %c discarded 6 aimed at %c.\n", 
                player + SHIFT, targetPlayer);
        fflush(stdout);
        
        //send thishappened
        send_this_happened(game, player + SHIFT, '6', targetPlayer, '-',
                '-', '-', '-');
    } else {
        //There was no target, just say discarded.
        fprintf(stdout, "Player %c discarded 6.\n", player + SHIFT);
        fflush(stdout);
        send_this_happened(game, player + SHIFT, '6', targetPlayer, '-',
                '-', '-', '-');
    }
}

/*
 * Triggered by card 5, target gets new card from deck. 
 * Where:
 * - player: the player making the move
 * - targetPlayer: the player being targeted
 * - holding: the card player is holding
 * - guess: the guess being made
 * If the player is holding a 7 then the process exits with MESSAGE_ERROR
 */ 
void new_card(struct Game* game, int player, char targetPlayer, 
        char holding, char guess) {
    //Get the next deckCard (note: it incremements the pos count)
    char deckCard = get_next_card(game);
    
    // if '-', wrap around the the first card
    if (deckCard == '-') {
        deckCard = game->deck->cards[0];
    }
    //can't keep 7 and throw out 6/5, not allowed a guess, must target self
    if (holding == '7' || targetPlayer == '-' || guess != '-') {
        safe_exit(game);
        exit_with(MESSAGE_ERROR);
    }
 
    //Send replace message to target
    fprintf(game->pipes[targetPlayer - SHIFT].write, "replace %c\n",
            deckCard);
    fflush(game->pipes[targetPlayer - SHIFT].write);
    
    //get card they are holding before it chagnes
    char oldCard = game->players[targetPlayer - SHIFT].holding;
    game->players[targetPlayer - SHIFT].holding = deckCard;
    
    //send discard information
    fprintf(stdout, "Player %c discarded 5 aimed at %c. This forced %c" 
            " to discard %c.", player + SHIFT, targetPlayer, targetPlayer, 
            oldCard);
    fflush(stdout);

    //check oldCard was 8 (they are outOfRound)
    if (oldCard == '8') {
        //They discarded an 8, so are out and do not get a card
        fprintf(stdout, " %c was out.\n", targetPlayer);
        game->players[targetPlayer - SHIFT].outOfRound = true;
        game->deck->pos--; //we just incremented the position
    } else {
        fprintf(stdout, "\n");
    }
    fflush(stdout);

    //send thishappened information
    if (oldCard != '8') {
        send_this_happened(game, player + SHIFT, '5', targetPlayer, '-',
                targetPlayer, oldCard, '-');
    } else {
        send_this_happened(game, player + SHIFT, '5', targetPlayer, '-',
                targetPlayer, oldCard, targetPlayer);
    }   
}

/*
 * Runs a compare card move for a valid targetPlayer, triggered by a 3.
 * Where:
 * - player: the player making the move
 * - targetPlayer: the player being targeted
 * - guess: the card being guessed
 * - compareTo: the card held by targetPlayer
 * - compareFrom: the card held by player
 */
void compare_valid_target(struct Game* game, int player, char targetPlayer,
        char guess, char compareTo, char compareFrom) {
    /*Varaibles: the loser and the discarded card */
    char loser;
    char discarded = '-';

    //Compare the two cards held by player and targetPlayer
    if (compareTo > compareFrom) {
        //player's card is greater
        game->players[player].outOfRound = true;
        loser = player + SHIFT;
        
        //get and send discarded message 
        char discarded = game->players[loser - SHIFT].holding;
        fprintf(stdout, "Player %c discarded 3 aimed at %c. This forced"
                " %c to discard %c. %c was out.\n", player + SHIFT, 
                targetPlayer, loser, discarded, loser);
        fflush(stdout);
        game->players[loser - SHIFT].outOfRound = true;
    } else if (compareFrom > compareTo) {
        //player's card is lower
        game->players[targetPlayer - SHIFT].outOfRound = true;
        loser = targetPlayer;
        
        //get and send discarded message
        char discarded = game->players[loser - SHIFT].holding;
        fprintf(stdout, "Player %c discarded 3 aimed at %c. This forced"
                " %c to discard %c. %c was out.\n", player + SHIFT, 
                targetPlayer, loser, discarded, loser);
        fflush(stdout);
        game->players[loser - SHIFT].outOfRound = true;
    } else {
        //The card is the same, no swap occurs
        loser = '-';
        fprintf(stdout, "Player %c discarded 3 aimed at %c.\n",
                player + SHIFT, targetPlayer);
        fflush(stdout); 
    }   

    //Send a thishappened message
    send_this_happened(game, player + SHIFT, '3', targetPlayer, '-', loser,
            discarded, loser);
} 

/*
 * Compare between a player and a targetPlayer, triggered by a 3, checks 
 * the conditions of targetPlayer and guess, if there is a valid target, 
 *  calls compare_valid_target
 * Where:
 * - player: the player who made the move
 * - targetPlayer: the player being targted
 * - guess: the card being guessed
 */ 
void compare(struct Game* game, int player, char targetPlayer, char guess)
{
    //The cards to compareTo and compareFrom
    char compareFrom = game->players[player].holding;       
    char compareTo = game->players[targetPlayer - SHIFT].holding;
    
    //check if no target when there can be
    //check if there is a guess
    if (guess != '-' || (targetPlayer == '-' && 
            target_available(game, player))) {
        safe_exit(game);
        exit_with(MESSAGE_ERROR);
    } 
    if (targetPlayer == '-') {
        fprintf(stdout, "Player %c discarded 3.\n", player + SHIFT);
        send_this_happened(game, player + SHIFT, '3', targetPlayer, '-', 
                '-', '-', '-');
        fflush(stdout);
    } else {
        compare_valid_target(game, player, targetPlayer, guess,
                compareTo, compareFrom);
    }
}

/*
 * Prints out status information and sends thishappened based on
 * a valid target (one which is not '-'). 
 */ 
void valid_target_guess(struct Game* game, int player, char targetPlayer,
        char guess) {
    //bool to indicate if the targetPlayer is out
    bool out = false;
    
    if (game->players[targetPlayer - SHIFT].holding == guess) {
        game->players[targetPlayer - SHIFT].outOfRound = true;
        out = true;
    }
    
    //If the target is out send appropriate message
    if (out) {
        fprintf(stdout, "Player %c discarded 1 aimed at %c guessing" 
                " %c. This forced %c to discard %c. %c was out.\n", 
                player + SHIFT, targetPlayer, guess, targetPlayer, 
                guess, targetPlayer);
        fflush(stdout);
        send_this_happened(game, player + SHIFT, '1', targetPlayer, 
                guess, targetPlayer, guess, targetPlayer);
    } else {

        //if guess is - then just say discarded card aimed at target
        if (guess == '-') {
            fprintf(stdout, "Player %c discarded 1 aimed at %c.\n",
                    player + SHIFT, targetPlayer);
            fflush(stdout);
            send_this_happened(game, player + SHIFT, '1', 
                    targetPlayer, '-', '-', '-', '-');
        } else {
        	//The target isn't out, send appropriate message
            fprintf(stdout, "Player %c discarded 1 aimed at %c "
                    "guessing %c.\n", player + SHIFT, targetPlayer, 
                    guess);
            fflush(stdout);
            send_this_happened(game, player + SHIFT, '1', targetPlayer,
                    guess, '-', '-', '-');
        }
    
    }
}

/*
 * Performs a guess move, triggered by a 1.  Where:
 * - player: the player who made the move
 * - targetPlayer: the target
 * - guess: the guess card
 * Exits program with MESSAGE_ERROR if guess is a '1' or error conditions
 * are met (such as targeting someone when no target is required)
 */ 
void guess_card(struct Game* game, int player, char targetPlayer, 
        char guess) {
    /*Set out boolean to tell if the targetPlayer is out*/
    //bool out = false;
    //guess cannot be 1, check target valid  
    if (guess == '1' || (targetPlayer == '-' && target_available(game,
            player) == true)) {
        safe_exit(game);
        exit_with(MESSAGE_ERROR);
    }

    if (targetPlayer != '-') {  
        //the target is valid, send to valid_target_guess
        valid_target_guess(game, player, targetPlayer, guess); 
    } else {
        //  The targetPlayer was just '-' so just say discarded
        //  guess must be -, as message of form 1--
        if (guess != '-') {
            safe_exit(game);
            exit_with(MESSAGE_ERROR);
        }
        fprintf(stdout, "Player %c discarded 1.\n", player + SHIFT);
        fflush(stdout);
        send_this_happened(game, player + SHIFT, '1', '-', '-', '-', 
                '-', '-');
    } 
}

/*
 * Conducts move with a card that does not require a targetPlayer.
 * Sends appropriate message to standard out and updates child processes.
 * Where:
 * - player: the player who made the move
 * - playedCard: the card that was played 
 */ 
void no_target_move(int player, char playedCard, struct Game* game) {
    //Send discard and thishappend message
    if (playedCard != '8' && playedCard != '1' && playedCard != '3' &&
            playedCard != '5' && playedCard != '6') {
        fprintf(stdout, "Player %c discarded %c.\n", player + SHIFT, 
                playedCard);
        fflush(stdout);
        send_this_happened(game, player + SHIFT, playedCard, '-', '-', '-',
                '-', '-');
    } 
    if (playedCard == '8') {
        //If playedCard was 8, then player is out
        fprintf(stdout, "Player %c discarded %c. %c was out.\n", 
                player + SHIFT, playedCard, player + 65);
        fflush(stdout);
        send_this_happened(game, player + SHIFT, playedCard, '-', '-',
                '-', '-', player + SHIFT);
        game->players[player].outOfRound = true;
    }
}

/*
 * Dispatches the move indicated by the playedCard to the appropraite
 * sub function.
 */ 
void dispatch_move(struct Game* game, int player, char targetPlayer,
        char holding, char playedCard, char guess) {
    switch(playedCard) {
        case '8':
            game->players[player].outOfRound = true; 
            break;
        case '7': //don'do anything, cases 5 and 6 check if 7 in pref to 5/
            break; 
        case '6': //swap cards w`ith target, check if holding 7
            swap(game, player, targetPlayer, holding, guess); 
            break; 
        case '5': //targeted player gets new card, check if hold 7
            new_card(game, player, targetPlayer, holding, guess); 
            break; 
        case '4':
            game->players[player].protected = true; 
            break; //player is immune, need to update state
        case '3': 
            compare(game, player, targetPlayer, guess); 
            break;
        case '2': 
            break;
        case '1': 
            guess_card(game, player, targetPlayer, guess); 
            break; 
        default: 
            //exit as a non-valid card was received
            safe_exit(game);
            exit_with(MESSAGE_ERROR);
            break;
    }
}

/*
 * Processes a move, updates the player's holding card and sends
 * replace to others as necessary. Where:
 * - given: is the card the player was given
 * - holding: is the card they are holding
 * - player: is the player who needs to be processed
 * - message: is the message received
 */ 
void process_move(struct Game* game, char given, char holding, 
        int player, char message[3]) {
    //the played card is the first char in message
    char playedCard = message[0];
    // if they were holding the played card then update the card
    // they are holding with the one they were given
    
    //check if they are not holding the card they played
    if (game->players[player].holding != playedCard &&
            given != playedCard) {
        safe_exit(game);
        exit_with(MESSAGE_ERROR);
    }
    if (game->players[player].holding == playedCard) {
        game->players[player].holding = given;
    }
    holding = game->players[player].holding;

    //extract targetPlayer and guess from message and check the bounds
    char targetPlayer = message[1];
    char guess = message[2];
    if (targetPlayer != '-' && (targetPlayer < 'A' || targetPlayer > 
            (game->numPlayers + SHIFT))) {
        safe_exit(game);
        exit_with(MESSAGE_ERROR);
    }
    if (playedCard > '9' || playedCard < '1') {
        safe_exit(game);
        exit_with(MESSAGE_ERROR);
    }
    if ((guess != '-' && (guess > '9' || guess < '0')) || 
            ((player + SHIFT) == targetPlayer && playedCard != '5')) {
        safe_exit(game);
        exit_with(MESSAGE_ERROR);
    }
 
    //if they were protected then they aren't any more
    game->players[player].protected = false;
    //can't target a protected player
    if (targetPlayer != '-' && 
            game->players[targetPlayer - SHIFT].protected == true) {
        safe_exit(game);
        exit_with(MESSAGE_ERROR);
    } 

    //dispatch the move to a sub function
    dispatch_move(game, player, targetPlayer, holding, playedCard, guess);
    
    //check if there was no target and act accordingly
    no_target_move(player, playedCard, game);
}

/*
 * Send winner information to stdout
 */ 
void send_winner(struct Game* game) {
    fprintf(stdout, "Winner(s):");
    
    //Progressively build the string
    for (int i = 0; i < game->numPlayers; i++) {
        if (game->scores[i] == 4) {
            fprintf(stdout, " %c", i + SHIFT);
        }
    }
    fprintf(stdout, "\n");
    fflush(stdout);
}

/*
 * Sets the scores for each player according to the cards they are
 * holding and whether they are out of the round
 */ 
void set_scores(struct Game* game) {
    /*Initialise the highCard to 1*/
    char highCard = '1';
                
    for (int i = 0; i < game->numPlayers; i++) {
        //get the highcard
        if (game->players[i].holding >= highCard &&
                game->players[i].outOfRound == false) {
            highCard = game->players[i].holding;
        }
    }   
    //Check if they are not holding the highCard
    for (int i = 0; i < game->numPlayers; i++) {
        if(game->players[i].holding != highCard) {
            game->players[i].outOfRound = true;
        }
    }
    //All who are still in get 1 added to their score
    for (int i = 0; i < game->numPlayers; i++) {
        if (!(game->players[i].outOfRound)) {
            game->scores[i] += 1;
        }
    }
}

/*
 * Runs a single round loop, handing cards to each player still in the
 * round in order, and responding to their messages. Returns boolean
 * to indicate if the round is over (true if it is, false if it is not).
 */ 
bool run_round(struct Game* game) {
    /*Variable declarations*/
    char card; //the card to send to a player
    bool roundOver = false; //the 
    char read, message[3];

    //Loop through each player and send them yourturn and process
    //the response
    for (int j = 0; j < game->numPlayers; j++) {
        if (game->players[j].outOfRound == false) {
            //get next card and check if '-' (means end of deck)
            if ((card = get_next_card(game)) == '-') {
                //The round is over, we have finished the deck
                roundOver = true;
                break;
            }
            // send card to player
            fprintf(game->pipes[j].write, "yourturn %c\n", card);
            fflush(game->pipes[j].write);
            int k = 0;
            read = fgetc(game->pipes[j].read);
            
            //loop through and get message from the player
            while(read != EOF && read != '\n') {
                //if k is 3 and read not \n then message from player
                //was no of form c1pc2 so exit with MESSAGE_ERROR
                if (k == 3 && read != '\n') {
                    safe_exit(game);
                    exit_with(MESSAGE_ERROR);
                } 
                message[k++] = read;
                read = fgetc(game->pipes[j].read);
            }
            //if read was EOF then a player quit
            if (read == EOF) {
                safe_exit(game);
                exit_with(QUIT_ERROR);
            }
            //process the move
            process_move(game, card, game->players[j].holding, 
                    j, message);
            //check for outOfRound
            roundOver = check_end_of_round(game);
            if (roundOver) {
                set_scores(game);
                break;
            }
        }
    }
    return roundOver;
}

/*
 * Runs the overall game. Loops until a player reachs four points.
 */
void play_game(struct Game* game) {
    //check if there is a winner
    bool winner = is_winner(game);
    
    //while two players still not eliminated  
    while (winner == false) {
        // set up bool to check end of round
        //send newround to each player
        char card;

        //start of a new round
        for(int i = 0; i < game->numPlayers; i++) {
            // if pos > 15 we need to reset otherwise out of bounds 
            game->players[i].outOfRound = false; //no one is out
            game->players[i].protected = false; //no one is protected
            card = get_next_card(game); //get the next card
            fprintf(game->pipes[i].write, "newround %c\n", 
                    card);  //give each player their card
            fflush(game->pipes[i].write); //fflush monkey
            game->players[i].holding = card;
        }   
        
        //check if roundOver
        bool roundOver = check_end_of_round(game);
        while(roundOver == false) {
            //send yourturn to each player in order and process their move
            roundOver = run_round(game); 
        }
        game->deck = game->deck->nextDeck;
        game->deck->pos = 1;
        //send scores to players and check if there is a game winner
        send_scores(game);
        winner = is_winner(game);
    }
    // send Winners message
    send_winner(game);
    
    //send gameover set each child, and then to be sure, kill them
    int childStatus;
    for (int i = 0; i < game->numPlayers; i++) {
        fprintf(game->pipes[i].write, "gameover\n");
        fflush(game->pipes[i].write);
        kill(children->pid[i], SIGKILL);
        waitpid(children->pid[i], &childStatus, 0); 
    }
    exit_with(NORMAL_EXIT);
}

/*
 * The main function
 */ 
int main(int argc, char** argv) {
    initialise_handler();
    if (argc < 4 || argc > 6) {
        exit_with(USAGE_ERROR);
    }
 
    //get the chosenFile from the input
    char* chosenFile = argv[1];
    FILE* deckFile = fopen(chosenFile, "r");
    Deck* deck = get_list_decks(deckFile);
    fclose(deckFile);
    
    //create enough space of the array of childPrograms
    char** childProgram = malloc((argc - 2) * sizeof(char*));
    //add process args to childProgram
    for (int j = 2; j < (argc); j++) {
        childProgram[j - 2] = argv[j];
    }

    //generate the game struct
    struct Game* game = malloc(sizeof(Game));
    game->round = 0; //initialise to round 0
    game->numPlayers = argc - 2; //set numPlayers
    game->deck = malloc(sizeof(Deck)); 
    game->deck = deck; //set deck(s) to use
    game->scores = malloc(sizeof(int) * (game->numPlayers)); //malscores  
    //array
    for (int i = 0; i < game->numPlayers; i++) {
        game->scores[i] = 0; //initialise all scores to 0
    }
    //create space for pipes and players
    game->pipes = malloc(sizeof(Stream) * (game->numPlayers)); 
    game->players = malloc(sizeof(Player) * (game->numPlayers)); 
    
    //Set up the children global variable to contain pid information on
    //players
    children = malloc(sizeof(ChildProcesses));
    children->pid = malloc(sizeof(int) * game->numPlayers);
    children->numChildren = game->numPlayers;
    
    //make children
    create_children(game, childProgram);
    //play the game
    play_game(game);    
    return 0;
}
