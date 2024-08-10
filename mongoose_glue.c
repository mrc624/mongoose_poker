// SPDX-FileCopyrightText: 2024 Cesanta Software Limited
// SPDX-License-Identifier: GPL-2.0-only or commercial
// Generated by Mongoose Wizard, https://mongoose.ws/wizard/

// Include your device-specific headers, and edit functions below
// #include "hal.h"

#include "mongoose_glue.h"
#include "mc_time.h"
#include "csv.h"
#include "poker.h"

#define POKER_NO_GAME "No Game"
#define POKER_DNE "Does Not Exist"

typedef enum{
  NO_CHANGE,
  ERROR_CHANGE,
  SUCCESS_CHANGE,
  NUM_MG_ERROR
} MG_ERROR_T;

enum poker_players
{
    PLAYER1,
    PLAYER2,
    PLAYER3,
    PLAYER4,
    PLAYER5,
    PLAYER6,
    PLAYER7,
    PLAYER8,
    PLAYER9,
    PLAYER10,
    POKER_PLAYER_MAX
};

static bool poker_in_progress = false;
static char current_players[POKER_PLAYER_MAX][POKER_NAME_LEN];
static __uint8_t num_poker_players;

static bool refreshing = false;

void glue_init(void) {
  MG_DEBUG(("Custom init done"));
}

// Authenticate user/password. Return access level for the authenticated user:
//   0 - authentication error
//   1,2,3... - authentication success. Higher levels are more privileged than lower
int glue_authenticate(const char *user, const char *pass) {
  int level = 0; // Authentication failure
  if (strcmp(user, "admin") == 0 && strcmp(pass, "admin") == 0) {
    level = 7;  // Administrator
  } else if (strcmp(user, "user") == 0 && strcmp(pass, "user") == 0) {
    level = 3;  // Ordinary dude
  }
  return level;
}

// refresh
bool glue_check_refresh(void) {
  return refreshing;
}
void glue_start_refresh(void) {
  refreshing = true;
  glue_update_state();
}

static struct time s_time = {"utc", "local", "up"};
void glue_get_time(struct time *data) {
  strncpy(data->local, get_local_time_string(), strlen(get_local_time_string()));
  strncpy(data->utc, get_utc_string(), strlen(get_utc_string()));
  strncpy(data->up, get_uptime_string(), strlen(get_uptime_string()));
  refreshing = false;
}
void glue_set_time(struct time *data) {
  s_time = *data;
}

static struct poker_run s_poker_run = {0, 0, "p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8", "p9", "p10", false, 0, "error", "success"};
MG_ERROR_T poker_run_error = NO_CHANGE;
void glue_get_poker_run(struct poker_run *data) {
  if(poker_in_progress)
  {
    printf("-----POKER IN PROGRESS-----\n");
    int i = 0;
    while(strcmp(current_players[i], "") != 0)
    {
      switch(i)
      {
        case PLAYER1:
          strncpy(s_poker_run.p1, current_players[PLAYER1], strlen(current_players[PLAYER1]));
          break;
        case PLAYER2:
          strncpy(s_poker_run.p2, current_players[PLAYER2], strlen(current_players[PLAYER2]));
          break;
        case PLAYER3:
          strncpy(s_poker_run.p3, current_players[PLAYER3], strlen(current_players[PLAYER3]));
          break;
        case PLAYER4:
          strncpy(s_poker_run.p4, current_players[PLAYER4], strlen(current_players[PLAYER4]));
          break;
        case PLAYER5:
          strncpy(s_poker_run.p5, current_players[PLAYER5], strlen(current_players[PLAYER5]));
          break;
        case PLAYER6:
          strncpy(s_poker_run.p6, current_players[PLAYER6], strlen(current_players[PLAYER6]));
          break;
        case PLAYER7:
          strncpy(s_poker_run.p7, current_players[PLAYER7], strlen(current_players[PLAYER7]));
          break;
        case PLAYER8:
          strncpy(s_poker_run.p8, current_players[PLAYER8], strlen(current_players[PLAYER8]));
          break;
        case PLAYER9:
          strncpy(s_poker_run.p9, current_players[PLAYER9], strlen(current_players[PLAYER9]));
          break;
        case PLAYER10:
          strncpy(s_poker_run.p10, current_players[PLAYER10], strlen(current_players[PLAYER10]));
          break;
      }
      i++;
    }
  }
  else
  {
    printf("-----POKER NOT IN PROGRESS-----\n");
    switch(poker_run_error)
    {
      case NO_CHANGE:
      printf("-----NO CHANGE-----\n");
        s_poker_run.players = 0;
        s_poker_run.buy = 0;
        sprintf(s_poker_run.p1, POKER_NO_GAME);
        sprintf(s_poker_run.p2, POKER_NO_GAME);
        sprintf(s_poker_run.p3, POKER_NO_GAME);
        sprintf(s_poker_run.p4, POKER_NO_GAME);
        sprintf(s_poker_run.p5, POKER_NO_GAME);
        sprintf(s_poker_run.p6, POKER_NO_GAME);
        sprintf(s_poker_run.p7, POKER_NO_GAME);
        sprintf(s_poker_run.p8, POKER_NO_GAME);
        sprintf(s_poker_run.p9, POKER_NO_GAME);
        sprintf(s_poker_run.p10, POKER_NO_GAME);
        sprintf(s_poker_run.error, " ");
        sprintf(s_poker_run.success, " ");
        break;
      case ERROR_CHANGE:
        printf("-----ERROR CHANGE-----\n");
        sprintf(s_poker_run.success, " ");
        break;
      case SUCCESS_CHANGE:
        printf("-----SUCCESS CHANGE-----\n");
        sprintf(s_poker_run.success, "Success");
        sprintf(s_poker_run.error, " ");
        break;
      case NUM_MG_ERROR:
        printf("-----NUM MG ERROR-----\n");
        break;
      default:
        printf("-----DEFAULT CASE-----\n");
        break;
    }
    printf("-----POKER RUN ERROR: %u-----\n", poker_run_error);
  }
  *data = s_poker_run;
}
void glue_set_poker_run(struct poker_run *data) {
  poker_run_error = NO_CHANGE;
  if(poker_in_progress)
  {

  }
  else
  {
    //Checking if any player entries match
    if((strcmp(data->p1, data->p2) == 0
              || strcmp(data->p1, data->p3) == 0
              || strcmp(data->p1, data->p4) == 0
              || strcmp(data->p1, data->p5) == 0
              || strcmp(data->p1, data->p6) == 0
              || strcmp(data->p1, data->p7) == 0
              || strcmp(data->p1, data->p8) == 0
              || strcmp(data->p1, data->p9) == 0
              || strcmp(data->p1, data->p10) == 0
              ) && strcmp(data->p1, POKER_NO_GAME) != 0)
    {
      poker_run_error = ERROR_CHANGE;
      sprintf(data->error, "P1 (%s) Entered Twice", data->p1);
    }
    else if((strcmp(data->p2, data->p3) == 0
              || strcmp(data->p2, data->p4) == 0
              || strcmp(data->p2, data->p5) == 0
              || strcmp(data->p2, data->p6) == 0
              || strcmp(data->p2, data->p7) == 0
              || strcmp(data->p2, data->p8) == 0
              || strcmp(data->p2, data->p9) == 0
              || strcmp(data->p2, data->p10) == 0
              ) && strcmp(data->p2, POKER_NO_GAME) != 0)
    {
      poker_run_error = ERROR_CHANGE;
      sprintf(data->error, "P2 (%s) Entered Twice", data->p2);
    }
    else if((strcmp(data->p3, data->p4) == 0
              || strcmp(data->p3, data->p5) == 0
              || strcmp(data->p3, data->p6) == 0
              || strcmp(data->p3, data->p7) == 0
              || strcmp(data->p3, data->p8) == 0
              || strcmp(data->p3, data->p9) == 0
              || strcmp(data->p3, data->p10) == 0
              ) && strcmp(data->p3, POKER_NO_GAME) != 0)
    {
      poker_run_error = ERROR_CHANGE;
      sprintf(data->error, "P3 (%s) Entered Twice", data->p3);
    }
    else if((strcmp(data->p4, data->p5) == 0
              || strcmp(data->p4, data->p6) == 0
              || strcmp(data->p4, data->p7) == 0
              || strcmp(data->p4, data->p8) == 0
              || strcmp(data->p4, data->p9) == 0
              || strcmp(data->p4, data->p10) == 0
              ) && strcmp(data->p4, POKER_NO_GAME) != 0)
    {
      poker_run_error = ERROR_CHANGE;
      sprintf(data->error, "P4 (%s) Entered Twice", data->p4);
    }
    else if((strcmp(data->p5, data->p6) == 0
              || strcmp(data->p5, data->p7) == 0
              || strcmp(data->p5, data->p8) == 0
              || strcmp(data->p5, data->p9) == 0
              || strcmp(data->p5, data->p10) == 0
              ) && strcmp(data->p5, POKER_NO_GAME) != 0)
    {
      poker_run_error = ERROR_CHANGE;
      sprintf(data->error, "P5 (%s) Entered Twice", data->p5);
    }
    else if((strcmp(data->p6, data->p7) == 0
              || strcmp(data->p6, data->p8) == 0
              || strcmp(data->p6, data->p9) == 0
              || strcmp(data->p6, data->p10) == 0
              ) && strcmp(data->p6, POKER_NO_GAME) != 0)
    {
      poker_run_error = ERROR_CHANGE;
      sprintf(data->error, "P6 (%s) Entered Twice", data->p6);
    }
    else if((strcmp(data->p7, data->p8) == 0
              || strcmp(data->p7, data->p9) == 0
              || strcmp(data->p7, data->p10) == 0
              ) && strcmp(data->p7, POKER_NO_GAME) != 0)
    {
      poker_run_error = ERROR_CHANGE;
      sprintf(data->error, "P7 (%s) Entered Twice", data->p7);
    }
    else if((strcmp(data->p8, data->p9) == 0
              || strcmp(data->p8, data->p10) == 0
              ) && strcmp(data->p8, POKER_NO_GAME) != 0)
    {
      poker_run_error = ERROR_CHANGE;
      sprintf(data->error, "P8 (%s) Entered Twice", data->p8);
    }
    else if((strcmp(data->p9, data->p10) == 0) && strcmp(data->p9, POKER_NO_GAME) != 0)
    {
      poker_run_error = ERROR_CHANGE;
      sprintf(data->error, "P9 (%s) Entered Twice", data->p9);
    }
    //Checking players exist
    else if(!player_exists(data->p1) && strcmp(data->p1, POKER_NO_GAME) != 0)
    {
      poker_run_error = ERROR_CHANGE;
      sprintf(data->error, "P%d %s", 1, POKER_DNE);
    }
    else if(!player_exists(data->p2) && strcmp(data->p2, POKER_NO_GAME) != 0)
    {
      poker_run_error = ERROR_CHANGE;
      sprintf(data->error, "P%d %s", 2, POKER_DNE);
    }
    else if(!player_exists(data->p3) && strcmp(data->p3, POKER_NO_GAME) != 0)
    {
      poker_run_error = ERROR_CHANGE;
      sprintf(data->error, "P%d %s", 3, POKER_DNE);
    }
    else if(!player_exists(data->p4) && strcmp(data->p4, POKER_NO_GAME) != 0)
    {
      poker_run_error = ERROR_CHANGE;
      sprintf(data->error, "P%d %s", 4, POKER_DNE);
    }
    else if(!player_exists(data->p5) && strcmp(data->p5, POKER_NO_GAME) != 0)
    {
      poker_run_error = ERROR_CHANGE;
      sprintf(data->error, "P%d %s", 5, POKER_DNE);
    }
    else if(!player_exists(data->p6) && strcmp(data->p6, POKER_NO_GAME) != 0)
    {
      poker_run_error = ERROR_CHANGE;
      sprintf(data->error, "P%d %s", 6, POKER_DNE);
    }
    else if(!player_exists(data->p7) && strcmp(data->p7, POKER_NO_GAME) != 0)
    {
      poker_run_error = ERROR_CHANGE;
      sprintf(data->error, "P%d %s", 7, POKER_DNE);
    }
    else if(!player_exists(data->p8) && strcmp(data->p8, POKER_NO_GAME) != 0)
    {
      poker_run_error = ERROR_CHANGE;
      sprintf(data->error, "P%d %s", 8, POKER_DNE);
    }
    else if(!player_exists(data->p9) && strcmp(data->p9, POKER_NO_GAME) != 0)
    {
      poker_run_error = ERROR_CHANGE;
      sprintf(data->error, "P%d %s", 9, POKER_DNE);
    }
    else if(!player_exists(data->p10) && strcmp(data->p10, POKER_NO_GAME) != 0)
    {
      poker_run_error = ERROR_CHANGE;
      sprintf(data->error, "P%d %s", 10, POKER_DNE);
    } //At this point we know that every player entered exists and none are duplicates, no errors
    else
    {
      num_poker_players = 0;
      if(strcmp(data->p1, POKER_NO_GAME) != 0)
      {
        strncpy(current_players[num_poker_players], data->p1, strlen(data->p1));
        num_poker_players++;
      }
      if(strcmp(data->p2, POKER_NO_GAME) != 0)
      {
        strncpy(current_players[num_poker_players], data->p2, strlen(data->p2));
        num_poker_players++;
      }
      if(strcmp(data->p3, POKER_NO_GAME) != 0)
      {
        strncpy(current_players[num_poker_players], data->p3, strlen(data->p3));
        num_poker_players++;
      }
     if(strcmp(data->p4, POKER_NO_GAME) != 0)
      {
        strncpy(current_players[num_poker_players], data->p4, strlen(data->p4));
        num_poker_players++;
      }
     if(strcmp(data->p5, POKER_NO_GAME) != 0)
      {
        strncpy(current_players[num_poker_players], data->p5, strlen(data->p5));
        num_poker_players++;
      }
     if(strcmp(data->p6, POKER_NO_GAME) != 0)
      {
        strncpy(current_players[num_poker_players], data->p6, strlen(data->p6));
        num_poker_players++;
      }
     if(strcmp(data->p7, POKER_NO_GAME) != 0)
      {
        strncpy(current_players[num_poker_players], data->p7, strlen(data->p7));
        num_poker_players++;
      }
     if(strcmp(data->p8, POKER_NO_GAME) != 0)
      {
        strncpy(current_players[num_poker_players], data->p8, strlen(data->p8));
        num_poker_players++;
      }
      if(strcmp(data->p10, POKER_NO_GAME) != 0)
      {
        strncpy(current_players[num_poker_players], data->p10, strlen(data->p10));
        num_poker_players++;
      } 
      if(num_poker_players > 1)
      {
        poker_run_error = SUCCESS_CHANGE;
        poker_in_progress = true;
      }
    }

    s_poker_run = *data;
  }
}

static struct poker_buyIn s_poker_buyIn = {false, "play", "error", "success"};
void glue_get_poker_buyIn(struct poker_buyIn *data) {
  *data = s_poker_buyIn;  // Sync with your device
}
void glue_set_poker_buyIn(struct poker_buyIn *data) {
  s_poker_buyIn = *data; // Sync with your device
}

static struct poker_indiv s_poker_indiv = {0, "p1", "p2", "error", "success"};
void glue_get_poker_indiv(struct poker_indiv *data) {
  *data = s_poker_indiv;  // Sync with your device
}
void glue_set_poker_indiv(struct poker_indiv *data) {
  s_poker_indiv = *data; // Sync with your device
}

static struct poker_end s_poker_end = {"p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8", "p9", "p10", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "error", "success"};
void glue_get_poker_end(struct poker_end *data) {
  *data = s_poker_end;  // Sync with your device
}
void glue_set_poker_end(struct poker_end *data) {
  s_poker_end = *data; // Sync with your device
}