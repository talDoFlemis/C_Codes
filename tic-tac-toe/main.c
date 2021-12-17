#include "stdio.h"
#include "curses.h"

char squares [10]={ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };
void drawBoard();
int checkwin();

int main(){
    int player = 1;
    int choice;
    char mark;
    int winCondition;

    do{
        drawBoard();

        printf("Player %u, please choose a squares: ", player);
        scanf("%u", &choice);
        mark = (player ==1)? 'X':'O';

        if(choice == 1 || choice == 2 || choice == 3 || choice == 4 || choice == 5 || choice == 6 || choice == 7 || choice == 8 || choice == 9 ){
            if(squares[choice] != 'X' && squares[choice] != 'O'){
                squares[choice] = mark;
                winCondition = checkwin();
                if(winCondition == -1){
                    player = (player == 1 )? 2:1;
                }else{}
            }else{
                printf("This option was alredy taken");
            }
        }else{
            printf("Please input a valid number");
        }
    }while (winCondition == -1);
    
    if(winCondition==1){
        printf("Congrats player %u, you win my menzola", player);
    }else{
        printf("We have a draw");
    }

    return 0;
}

int checkwin(){
    if(squares[1]==squares[2] && squares[2]==squares[3])
        return 1;

    else if (squares[4] == squares[5] && squares[5] == squares[6])
        return 1;
        
    else if (squares[7] == squares[8] && squares[8] == squares[9])
        return 1;
        
    else if (squares[1] == squares[4] && squares[4] == squares[7])
        return 1;
        
    else if (squares[2] == squares[5] && squares[5] == squares[8])
        return 1;
        
    else if (squares[3] == squares[6] && squares[6] == squares[9])
        return 1;
        
    else if (squares[1] == squares[5] && squares[5] == squares[9])
        return 1;
        
    else if (squares[3] == squares[5] && squares[5] == squares[7])
        return 1;
        
    else if (squares[1] != '1' && squares[2] != '2' && squares[3] != '3' && squares[4] != '4' && squares[5] != '5' && squares[6] != '6' && squares[7]  != '7' && squares[8] != '8' && squares[9] != '9')
        return 0;
    else
        return -1;
}

void drawBoard(){
    system("clear");
    printf("\n\n\tTic Tac Toe\n\n");

    printf("Player 1 (X)  -  Player 2 (O)\n\n\n");


    printf("     |     |     \n");
    printf("  %c  |  %c  |  %c \n", squares[1], squares[2], squares[3]);

    printf("_____|_____|_____\n");
    printf("     |     |     \n");

    printf("  %c  |  %c  |  %c \n", squares[4], squares[5], squares[6]);

    printf("_____|_____|_____\n");
    printf("     |     |     \n");

    printf("  %c  |  %c  |  %c \n", squares[7], squares[8], squares[9]);

    printf("     |     |     \n\n");
}