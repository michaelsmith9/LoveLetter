/*
 * The player program
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>

/*Defined constants */
#define USAGE_NUMBER 3 //the correct number of args for the program
#define FIRST 0 //index 0 in array is first position
#define SECOND 1 //index 1 is second position
#define MAX_CARDS 16 //The max cards a player can play in a two player game
/*Exit condition values*/
#define NORMAL_EXIT 0
#define USAGE_EXIT 1
#define COUNT_EXIT 2
#define ID_EXIT 3
#define HUB_EXIT 4
#define MESSAGE_EXIT 5
/*Common shifts for ints and chars*/
#define SHIFT_LETTER 65
#define SHIFT_NUMBER 48

/* The ThisPlayer struct (known for each instance of player process)
 * - label: the player's label
 * - cards: the cards the player is holding
 * - numberOthers: the number of players in the game
 * - addNext: which card to add next (either 1 or 0, index of cards)
 */
typedef struct {
    char label; 
    int cards[2]; 
    int numberOthers; 
    int addNext; 
} ThisPlayer;

/* A generic player struct (one generated per player, including this one) 
 * - label: the player's label
 * - protected: if the player is protected (last discarded a 4)
 * - outOfRound: if the player is out of the current round
 * - playedCards: an array of the cards played by this player
 * - numberPlayed: the number of cards this player has played
 */
typedef struct {
    char label; 
    bool protected; 
    bool outOfRound; 
    int playedCards[MAX_CARDS]; 
    int numberPlayed; 
} Player;

/*
 * Exits the process with the given exitStatus.
 */ 
void exit_with(int exitStatus) {
    switch(exitStatus) {
        case 0:
            exit(0);
            break;
        case 1:
            fprintf(stderr, "Usage: player number_of_players myid\n");
            exit(1);   
            break;
        case 2:
            fprintf(stderr, "Invalid player count\n");
            exit(2);
            break;
        case 3:
            fprintf(stderr, "Invalid player ID\n");
            exit(3);
            break;
        case 4:
            fprintf(stderr, "Unexpected loss of hub\n");
            exit(4);
            break;
        case 5:
            fprintf(stderr, "Bad message from hub\n");
            exit(5);
            break;
        default: 
            break;
    }
}

/*
 * Extracts player status information from players and prints it to 
 * standard error.
 */
void print_status(Player *players, ThisPlayer *thisPlayer) {
    /* The designator for a player's state (outOfRound or eliminated and an 
    * array of chars for the cards played) */
    char designator, string[8];
    char cardOne = (char)thisPlayer->cards[0]; //thisPlayer's first card
    char cardTwo = (char)thisPlayer->cards[1]; //thisPlayer's second card
    
    /* Loop through each player and print out status information */
    for (int i = 0; i < thisPlayer->numberOthers; i++) {
        //set the designator according to if they are outOfRound or
        //protected, or neither
        if (players[i].outOfRound) {
            designator = '-';
        } else if (players[i].protected) {
            designator = '*';
        } else {
            designator = ' ';
        }
        //add in the cards they played to the string
        for (int j = 0; j < players[i].numberPlayed; j++) {
            string[j] = players[i].playedCards[j] + SHIFT_NUMBER;
        }
        //terminate the string with the null character
        string[players[i].numberPlayed] = '\0';
        fprintf(stderr, "%c%c:%s\n", players[i].label, 
                designator, string);
    }
    
    /* Generate you are holding information */
    //if card is zero, then print a dash thisPlayer is not holding anything
    if (cardOne == 0) {
        cardOne = '-';
    } 
    if (cardTwo == 0) {
        cardTwo = '-';
    }
    fprintf(stderr, "You are holding:%c%c\n", cardOne, cardTwo);    
}

/*
 * Update the state of the given players (represented by chars):
 * moveMaker and eliminatedPlaer to indicate that eliminatedPlayer was 
 * eliminated and that moveMaker played the card cardPlayed. 
 */ 
void update_move_maker(ThisPlayer *thisPlayer, Player *players,
        char moveMaker, char eliminatedPlayer, char cardPlayed) {
    /* The index for the next card played by the moveMaker */
    int numMakerPlayed = players[moveMaker - SHIFT_LETTER].numberPlayed;

    /* Add in the char cardPlayed to the list of playedCards*/
    players[moveMaker - SHIFT_LETTER].playedCards[numMakerPlayed] =
            cardPlayed - SHIFT_NUMBER;
    players[moveMaker - SHIFT_LETTER].numberPlayed += 1;

    // If they were protected, they aren't any more
    if(players[moveMaker - SHIFT_LETTER].protected == true) {
        players[moveMaker - SHIFT_LETTER].protected = false;
    }

    // If they played a 4, they are protected
    if (cardPlayed == '4') {
        players[moveMaker - SHIFT_LETTER].protected = true;
    }
    
    // If eliminatedPlayer was a player, they are out of the round
    if (eliminatedPlayer != '-') {
        players[eliminatedPlayer - SHIFT_LETTER].outOfRound = true;
    }
}

/*
 * Checks a string (message) of the form pcpc/pcp to determine if any
 * character in the string is out of the acceptable range. 
 * Acceptable range for a p:
 *  - either '-' or between 'A' and the upper limit of players in the game
 *  (so in a 3 player game between 'A' and 'C')
 * Acceptable range for a c:
 * - either '-' or between '1' and '8' inclusive
 * Returns a boolean to indicate the validity of the string. True is
 * valid, false is not.
 */
bool check_string(char *message, int numberPlayers) {
    /* Variable definitions */
    bool returnValue = true;
    int playerChecks[] = {0, 2, 5, 7}; //indexes of players in message
    int cardChecks[] = {1, 3, 6}; //indexes of cards in message

    // Check: message length, and that char at index 4 is a '/'  
    if (strlen(message) != 8 || message[4] != '/') {
        returnValue = false;
    }

    //check all p's (players) in thishappened are in the right range
    for (int i = 0; i < 4; i++) {
        if (message[playerChecks[i]] != '-' && (message[playerChecks[i]] 
                < 'A' || message[playerChecks[i]] > 
                (numberPlayers + SHIFT_LETTER - 1))) {
            returnValue = false;
        }
    }
    //check all cards in thishappened are in the right range
    for (int i = 0; i < 3; i++) {
        if (message[cardChecks[i]] != '-' && (message[cardChecks[i]] < '1' 
                || message[cardChecks[i]] > '8')) {
            returnValue = false;
        }
    }

    return returnValue;
}

/*
 * Update the state of the game after thishappend has been received.
 * Checks range of all values in message which is of the form:
 * 'p1c1p2c2/p3c3p4\n'
 * Where:
 * - p1: the player who made the move
 * - c1: the card that was played by p1
 * - p2: the player who was targeted
 * - c2: the card that was guessed
 * - p3: the player who dropped card c3
 * - c3: the card that was dropped by p3
 * - p4: the player who was eliminated  
 */
void update_state(char *message, ThisPlayer *thisPlayer, Player *players) {
    /* Declarations of variables from message */
    char moveMaker = message[0]; //player who played card
    char cardPlayed = message[1]; //card that was played
    char eliminatedPlayer = message[7]; //the elimated player    
    
    //checkk the message does not contain bad chars
    if (!(check_string(message, thisPlayer->numberOthers))) {
        exit_with(MESSAGE_EXIT);
    }
  
    //if moveMaker is not this player (as internal state updated before
    //thishappened message), then update that player's information  
    if (!(moveMaker == thisPlayer->label)) {
        //get the current number played and add it to the cards played
        update_move_maker(thisPlayer, players, moveMaker, 
                eliminatedPlayer, cardPlayed);
    }
    
    //the player who dropped a card
    char dropper = message[5];
    if (dropper >= 'A' && dropper <= 'D') {
        char cardDropped = message[6];
        int numDropperPlayed = players[dropper - SHIFT_LETTER].numberPlayed;
        players[dropper - SHIFT_LETTER].playedCards[numDropperPlayed] = 
                cardDropped - SHIFT_NUMBER;
        players[dropper - SHIFT_LETTER].numberPlayed += 1;
        if (cardDropped == '4') {
            players[dropper - SHIFT_LETTER].protected = true;
        }
    } else if (dropper != '-') {
        exit_with(MESSAGE_EXIT);
    }

    //Update the eliminatedPlayer's information 
    if (eliminatedPlayer >= 'A' && eliminatedPlayer <= ('A' + 
            thisPlayer->numberOthers)) {
        players[eliminatedPlayer - SHIFT_LETTER].outOfRound = true;
        // if thisPlayer is the eliminatedPlayer
        if (thisPlayer->label == eliminatedPlayer) {
            thisPlayer->cards[0] = 0;
            thisPlayer->addNext = 0;
            thisPlayer->cards[1] = 0;
        }
    } else if (eliminatedPlayer != '-') {
        exit_with(MESSAGE_EXIT);
    }
}

/*
 * Adds the given card to thisPlayer's hand, with newRound indicating
 * if a newRound has begun.
 */
void add_card(char card, bool newround, ThisPlayer *thisPlayer,
        Player * players) {
    /*Variable declaration: next is the next position to add a card*/
    int next = thisPlayer->addNext;
    
    //check if card not in range
    if (card < '1' || card > '8') {
        exit_with(MESSAGE_EXIT);
    }
    
    //check if newround, if it is, set protection and outOfRound to false
    if (newround) {
        thisPlayer->cards[0] = card;
        thisPlayer->cards[1] = 0;
        for (int i = 0; i < thisPlayer->numberOthers; i++) {
            players[i].protected = false;
            players[i].outOfRound = false;
            players[i].numberPlayed = 0;
            for (int j = 0; j < 8; j++) {
                players[i].playedCards[j] = 0;
            }
        }
    } else {
        if (thisPlayer->cards[0] != 0) {
            thisPlayer->cards[next] = card;
        } else {
            thisPlayer->cards[0] = card;
        }
    }
    
    //next time add to position 1
    thisPlayer->addNext = 1;
}

/*  
 *  Returns the card that should be guessed. Returns '-' unless the card
 *  is a 1. If the card is a 1, guess is chosen by choosing the highest
 *  card that has not yet been played.
 */
int get_guess(int choice, ThisPlayer *thisPlayer, Player *players) {
    //if choice is not one, then return '-' as there is no guess
    if (choice != 1) {
        return (int)'-';
    } else {
        // Set up that maximum playable numbers for each card
        int max[] = {5, 2, 2, 2, 2, 1, 1, 1};
        bool isAllPlayed[8];
        int numberPlayed[8];
        int card;
        int returnCard = 0;
        
        //get how many of each card have been played    
        for (int i = 0; i < thisPlayer->numberOthers; i++) {
            for(int j = 0; j < 16; j++) {
                card = players[i].playedCards[j];
                if (card != 0) {
                    numberPlayed[card - 1] += 1;
                }
            }

        }
        
        //check if the maximum number has been played
        for (int m = 0; m < 8; m++) {
            if (numberPlayed[m] == max[m]) {
                isAllPlayed[m] = true;
            }
        }   
        
        //get the highest card that has not all been played
        for (int n = 7; n > 0; n--) {
            if (isAllPlayed[n] == false) {
                returnCard = n + 1;
                break;
            }
        }
        //cannot guess 1
        if (returnCard == 0) {
            return '-';
        } else {
            return returnCard + 48;
        }
    }
}

/*
 * Perform a discard by sending a message to standard out of the form
 * c1pc2 where c1 is the choice, p is the label of the target  and c2 
 * is the guess. Also sends to stderr a message of the form: To hub:c1pc2\n
 */ 
void discard(int choice, char guess, char label, ThisPlayer *thisPlayer) {
    fprintf(stdout, "%d%c%c\n", choice, label,
            guess);
    fflush(stdout);
    fprintf(stderr, "To hub:%d%c%c\n", choice, label,
            guess);
}

/*
 * Find and target a player given the choice. Calls get_guess to get the
 * guess card for the choice. Target is chosen by selecting the first
 * unprotected and still playing player following thisPlayer. For example,
 * if thisPlayer's label is B in a 4 player game, then C and D will be 
 * checked as targets before A is. If no target is found, then we target
 * no one. 
 */ 
void target_player(Player *players, ThisPlayer *thisPlayer, int choice) {
    /*The guess to be made*/
    int guess;
    bool targetSomeone = false; //no one has been targeted
    //new round, no longer protected
    players[thisPlayer->label - SHIFT_LETTER].protected = false;
    //Check all players after thisPlayer in designator (i.e. if we are
    //B then check C and D - assuming 4 players)
    for (int i = (thisPlayer->label - SHIFT_LETTER) + 1; i <
            thisPlayer->numberOthers; i++) {
        //if unprotected and not out of round
        if (!(players[i].protected) && !(players[i].outOfRound)) {
            //target them by getting the guess and discarding the choice
            guess = get_guess(choice, thisPlayer, players);
            discard(choice, guess, players[i].label, thisPlayer);
            //someone has been targeted
            targetSomeone = true;
            break;
        } 
    }
    //if there is no one to target we must target no-one if
    //1,3 or 6. If the card is a 5, we target ourselves
    for (int i = 0; i < thisPlayer->label - SHIFT_LETTER + 1; i++) {
        if (!(players[i].protected) && !(players[i].outOfRound) &&
                targetSomeone == false) {
            guess = get_guess(choice, thisPlayer, players);
    
            //if we are targeting ourselves
            if (i == (thisPlayer->label - SHIFT_LETTER) && choice == 5) {
                //if choice is a 5 we are targeting ourself
                discard(choice, '-', thisPlayer->label, thisPlayer);
            } else if (i == (thisPlayer->label - SHIFT_LETTER) && 
                    choice != 5) {
                //choice is not a 5,we target no one, but still need guess 
                //the guess will be '-' unless choice is 1
                discard(choice, '-', '-', thisPlayer);
            } else {
                //player is not us, we can target them
                discard(choice, guess, players[i].label, thisPlayer);
            }
            targetSomeone = true;
            break;
        }
    }
}

/*
 * Update the internal state of this player according to the choice made.
 * The number of cards played is increased for the corresponding Player
 * in players.
 */ 
void update_internal_state(Player *players, ThisPlayer *thisPlayer,
        int choice) {
    // get the player designator as an int and the number of cards played
    int player = thisPlayer->label - SHIFT_LETTER;
    int numberPlayed = players[player].numberPlayed;
    
    // update the corresponding Player for thisPlayer in players
    players[player].playedCards[numberPlayed] = choice;
    players[player].numberPlayed += 1;

    //If we chose 4 we are protected, if not we are not
    if (choice == 4) {
        players[player].protected = true;
    } else if (choice != 4 && players[player].protected == true) {
        players[player].protected = false;
    }
}

/*
 * Chooses which of two held cards to discard to stdout. Takes two cards
 * held by thisPlayer and chooses card to discard based on following steps:
 * 1. if one is a seven, it is discarded.
 * 2. the lower of the two is chosen.
 * 3. if a target is required, then target_player is called.
 * 4. if a guess is required, then target_player is called.
 */
void make_move(Player *players, ThisPlayer *thisPlayer) {
    //Get the first and second cards held by thisPlayer
    int firstCard = thisPlayer->cards[FIRST] - 48;
    int secondCard = thisPlayer->cards[SECOND] - 48;
    int choice; //the card chosen to discard
    
    //if you hold 7 and either 5 or 6, then discard 7
    if (firstCard == 7 && (secondCard == 5 || secondCard == 6)) {
        thisPlayer->cards[FIRST] = secondCard + 48; //Firstcard is second
        thisPlayer->cards[SECOND] = 0; //second is 0
        discard(firstCard, '-', '-', thisPlayer); //discard the card
        choice = firstCard;
    } else if (secondCard == 7 && (firstCard == 5 || firstCard == 6)) {
        thisPlayer->cards[FIRST] = firstCard + 48;
        thisPlayer->cards[SECOND] = 0;
        discard(secondCard, '-', '-', thisPlayer); //discard the card
        choice = secondCard;
    } else {
        //choose the lower of the two cards
        if (firstCard < secondCard || secondCard < 0) { // < 0 if card was -
            choice = firstCard;
        } else {
            choice = secondCard;
        }
        
        //if choice card needs a target then target a player
        if (choice == 1 || choice == 3 || choice == 6 || choice == 5) {
            //target next unprotected player
            target_player(players, thisPlayer, choice);
            //Update the state of this player's hand
            if (firstCard == choice) {
                thisPlayer->cards[FIRST] = thisPlayer->cards[SECOND];
                thisPlayer->cards[SECOND] = 0;
            } else if (secondCard == choice) {
                thisPlayer->cards[SECOND] = 0;
            }
        } else {
            //card does not need a target
            discard(choice, '-', '-', thisPlayer);
            if (firstCard == choice) {
                thisPlayer->cards[FIRST] = thisPlayer->cards[SECOND];
                thisPlayer->cards[SECOND] = 0;
            } else if (secondCard == choice) {
                thisPlayer->cards[SECOND] = 0;
            }
        }
    }
    //update the state of thisPlayer
    update_internal_state(players, thisPlayer, choice); 
}

/*
 * Check the string scoreMessage is appropriate to the number numberPlayers
 * and that the scores are within the valid range of 0 to 4 inclusive.
 * Exits the program if error found. A valid score message is of the form:
 * - scores i j...\n -> with i j... equal to numberPlayers. 
 * In a 3 player game, scores will be of form: scores i j k\n.
 */
void check_score_message(char *scoreMessage, int numberPlayers) {
    //copy the string
    char *thisCopy = malloc(sizeof(char) * strlen(scoreMessage));
    thisCopy = strcpy(thisCopy, scoreMessage);
 
    //split the string
    char *secondArg = strtok(scoreMessage, " ");
    secondArg = strtok(NULL, " ");
    int scoreSize = 6; //size of scores string

    //check range of second argument of string (which is the first score)
    if (strlen(secondArg) != 1 || secondArg[0] < '0' || 
            secondArg[0] > '4') {
        exit_with(MESSAGE_EXIT);
    }
    int numScores = 1; //already one score
    char* strScore;

    //loop through the rest of the message, check range of scores and
    //the number of them
    while ((strScore = strtok(NULL, " ")) != NULL) {
        numScores++;
        if (strlen(strScore) != 1 || strScore[0] > '4' || 
                strScore[0] < '0') {
            exit_with(MESSAGE_EXIT);
        }
    }
    //test length of the string to make sure it is the length of 'scores '
    //and the number of players (i.e. in 2 player game: 'scores' + ' 1'
    //+ ' 2' gives length of 6 + 2 + 2 = 10
    if (strlen(thisCopy) != (scoreSize + numberPlayers * 2)) {
        exit_with(MESSAGE_EXIT);
    }
    //if there are more scores then players, then exit
    if (numScores > numberPlayers) {
        exit_with(MESSAGE_EXIT);
    }
}

/*
 * Parses the string given by message and calls the appropriate
 * function to handle the input. Checks the size of the input, exits
 * the program if no match found. Match cases are: 
 * - newround c\n
 * - yourturn c\n
 * - thishappened pcpc/pcp\n
 * - replace c\n
 * - scores i j...\n -> to number of players
 */ 
void parse_message(Player *players, ThisPlayer *thisPlayer, 
        char *message) {
    //copy for checking the length
    char *keepCopy = malloc(sizeof(char) * strlen(message));
    keepCopy = strcpy(keepCopy, message);
    char *token; //a sub part of a string
    token = strtok(message, " "); //split string
    char *secondArg = strtok(NULL, " "); //get second argument
    
    //check if secondArg too long (must be single char or of form pcpc/pcp
    //for thishappened)
    if ((secondArg != NULL && strlen(secondArg) != 1 && 
            strlen(secondArg) != 8) || secondArg == NULL) {
        exit_with(MESSAGE_EXIT);
    }
    
    //parse message
    if (strcmp(token, "newround") == 0) {
        // add card to hand, pass true as it is a new round
        add_card(secondArg[0], true, thisPlayer, players);
    } else if (strcmp(token, "yourturn") == 0) {
        // add card (c) and make a move
        add_card(secondArg[0], false, thisPlayer, players);
        print_status(players, thisPlayer); //print second status info
        make_move(players, thisPlayer);
    } else if (strcmp(token, "thishappened") == 0) {
        //update state of other players
        update_state(secondArg, thisPlayer, players);
    } else if (strcmp(token, "replace") == 0) {
        if (secondArg[0] < '1' || secondArg[0] > '8' || 
                strlen(keepCopy) != 9) {
            exit_with(MESSAGE_EXIT);
        } 
        //replace card we are holding
        thisPlayer->cards[FIRST] = secondArg[FIRST]; 
    } else if (strcmp(token, "scores") == 0) {
        //check the score message
        check_score_message(keepCopy, thisPlayer->numberOthers);
    } else {
        //it was none of the above, exit with message error
        exit_with(MESSAGE_EXIT);
    }
}

/*
 * Runs a game by continuously reading in from stdin and processing
 * the message. Exits program on reception of 'gameover\n' or EOF.
 */
void run_game(Player *players, ThisPlayer *thisPlayer) {
    /*variable declarations*/
    bool gameover = false; //gameover boolean
    char input[23], c, *message; //generic char c, *in is message in
    int readIn; //counts number of chars read in from stdin
   
    while (gameover == false) {
        readIn = 0; //reset input array and readIn
        for (int j = 0; j < 23; j++) {
            input[j] = 0;
        }
        c = fgetc(stdin); //get first char

        //get input
        while(c != '\n' && c != EOF) {
            input[readIn++] = c;
            // if i >= 22 and no \n then bad message
            if (readIn == 22) {
                input[--readIn] = '\0';
                fprintf(stderr, "From hub:%s\n", input);
                print_status(players, thisPlayer);
                exit_with(MESSAGE_EXIT);
            }
            c = fgetc(stdin);
        }
 
        //check for EOF
        if (c == EOF) {
            exit_with(HUB_EXIT);
        }
        
        //send from hub message
        fprintf(stderr, "From hub:%s\n", input);
        message = input;
   
        //print the first set of status information
        print_status(players, thisPlayer);
        //check if it was gameover message
        if (strcmp(message, "gameover") == 0) {
            gameover = true;
            exit_with(NORMAL_EXIT);
        }
   
        //parse the given message
        parse_message(players, thisPlayer, message);
        //print status information
        print_status(players, thisPlayer);
    }
}

/*
 * The main function
 */ 
int main(int argc, char **argv) {
    // on startup print single '-' to stdout
    fprintf(stdout, "-");
    fflush(stdout);

    //generate signal handler to ignore SIGPIPE
    struct sigaction sigPipeHandler;
    sigPipeHandler.sa_handler = SIG_IGN; 
    sigPipeHandler.sa_flags = SA_RESTART;
    sigaction(SIGPIPE, &sigPipeHandler, 0);
        
    // check for incorecct number of args
    if (argc != USAGE_NUMBER) {
        exit_with(USAGE_EXIT);
    } 
    
    // get the number of players and label for this player
    int numberPlayers = atoi(argv[1]); //the number of players
    char label = argv[2][0]; //the label for this player 

    //check that only 1 character is inputted
    if (strlen(argv[1]) != 1 || numberPlayers < 2 || numberPlayers > 4) {
        exit_with(COUNT_EXIT);
    }
   
    //check that only one character is inputed and the type of character
    if (strlen(argv[2]) != 1 || label < 'A' || label > 'D' || label > 
            (numberPlayers + 64)) {
        exit_with(ID_EXIT);
    }
    
    // create thisPlayer struct
    ThisPlayer *thisPlayer = malloc(sizeof(ThisPlayer*));
    thisPlayer->label = label;
    thisPlayer->numberOthers = numberPlayers;
    thisPlayer->addNext = 1;
    
    //create an array of player structs for the other players
    Player *players = malloc(numberPlayers * sizeof(Player));
    for (int i = 0; i < numberPlayers; i++) {
        players[i].label = 'A' + i; //set label
        players[i].protected = false; //all unprotected
        players[i].outOfRound = false; //all in round
        players[i].numberPlayed = 0; //all played 0 cards
    }
    
    //run the game
    run_game(players, thisPlayer);
    exit(NORMAL_EXIT);
}
