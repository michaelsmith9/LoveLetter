An implementation of the LoveLetter card game in C. The game is played 
between multiple processes with no user input aside from initialisation.

This was develoepd as part of CSSE2310. 

The card game is run in a series of rounds with the cards coming from a given
deck file. Each round has a winner(s). At the end of all rounds, the player
with the highest points wins. 

This game is best played in a UNIX environment. 

The process for the game is as follows:

At the beginning of each round, the first card is taken from the deck and set 
aside. Next, one card is given to each player in turn. 
Beginning with Player A, each player takes their turn:
1. They are given an additional card from the deck.
2. They then choose one of the two cards they are holding to discard, they keep the other card.
3. This might result in one of the players being eliminated from the round (they can play again at the start of the next round).

The hub program runs the card game and interacts with multiple players.

The cards have the following effects:

8. If a player ever discards this card, they are out of the round immediately.
7. This card doesn’t do anything when discarded. However, it must be discarded 
    in preference to 6 or 5. That is, you can’t keep 7 and throw out 6 or 5.
6. When this card is discarded during your turn, you pick (target) another 
    player. The two of you swap cards (each of you only have one card at this 
    point).
5. When this card is discarded [during your turn], you pick (target) a player 
    (you are allowed to target yourself). The targeted player must discard the
    card they are holding and get a new one from the deck. If the deck is empty,
they are given the card which was put aside at the beginning of the round.
4. When this card is discarded, the player can not be targeted until the start 
    of their next turn.
3. When this card is discarded [during your turn], you target another player. 
    The two of you compare cards, if one of you has a lower card, they are out of the round.
2. This card has no effect when discarded.
1. When this card is discarded [during your turn], target another player
    and guess what they are holding. If you guess correctly, they are out of 
the round. If not, nothing happens. You are not permitted to guess that they
are holding 1.

Enjoy!
