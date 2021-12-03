#include <iostream>
#include <string>
#include <stdlib.h>
#include <vector>
#include <time.h>
#include <math.h>

#ifdef WIN64
  #include <conio.h>
#endif

#ifdef __linux__
  #include <termios.h>
  static struct termios old, current;
  int _getch(){
    tcgetattr(0, &old);
    current = old;
    current.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &current);
    int key = getchar();
    tcsetattr(0, TCSANOW, &old);
    return key;
  }

#endif

using namespace std;

#define bombratio 0.1f

#define iomovedown_r(n) printf("\x1b[%dE", n)
#define iomoveup_r(n) printf("\x1b[%dF", n)
#define iomoveright(n) printf("\x1b[%dC", n)
#define iomoveleft(n) printf("\x1b[%dD", n)
#define iomoveup(n) printf("\x1b[%dA", n)
#define iomovedown(n) printf("\x1b[%dB", n)
#define iomovepos(x, y) if(x > 0) iomoveright(x); else if(x < 0) iomoveleft(abs(x)); if(y > 0) iomovedown(y); else if(y < 0) iomoveup(abs(y))
#define iomoveupdown_r(n) if(n > 0) iomovedown_r(n); else if(n < 0) iomoveup_r(abs(n))
#define iomoveupdown(n) if(n > 0) iomovedown(n); else if(n < 0) iomoveup(abs(n))
#define iomoverightleft(n) if(n > 0) iomoveright(n); else if(n < 0) iomoveleft(abs(n))

#define isinbounds(pos, size) (pos.x >= 0 && pos.y >= 0 && pos.x < size.x && pos.y < size.y)
#define constraint(min, max, n) n >= min? n < max? n: max: min 

#define state_opened 1
#define state_closed 0
#define state_flagged 2

#define char_state_closed "."
#define char_state_flagged "F"
#define char_state_bomb "B"
#define char_state_opened_null " "

#define gamestate_playing 0
#define gamestate_quitting 1
#define gamestate_gameover 2
#define gamestate_gamewin 3

#define keypress_left 'a'
#define keypress_right 'd'
#define keypress_up 'w'
#define keypress_down 's'
#define keypress_confirm 'e'
#define keypress_flag 'f'
#define keypress_quit 'q'

#define offset_scoreboard_to_playspace 2
#define offset_playspace_to_eventbar 3

#define debug_showbomb false
#define debug_shownums false

#define minesweeper_spacesize_default vec2<int>{16, 16}


//coordinate system are the same, except the y axis is upside down
template<typename t> class vec2{
  public:
    t x,y;
    vec2(t xn = t{}, t yn = t{}){
      x = xn;
      y = yn;
    }

    vec2<t> operator+(vec2<t> v1){
      vec2<t> res;
      res.x = x + v1.x;
      res.y = y + v1.y;
      return res;
    }

    vec2<t> operator-(vec2<t> v1){
      vec2<t> res;
      res.x = x - v1.x;
      res.y = y - v1.y;
      return res;
    }
};


class cursorHandler{
  private:
    vec2<int> currentOffset = vec2<int>{0,0}, currentPos = vec2<int>{0,0};

  public:
    cursorHandler(vec2<int> offset = vec2<int>{0,0}){
      changeOffset(offset);
    }

    ~cursorHandler(){
      //cout << currentPos << " " << currentOffset << endl;
      changeOffset(vec2<int>{0,0});
      printf("\n");
    }

    void changeOffset(vec2<int> offset){
      offset = offset - currentOffset;
      iomovepos(offset.x, offset.y);
      vec2<int> pos = currentPos;
      currentPos = vec2<int>(0,0);
      currentOffset = offset;
      changeCursorPos(pos);
    }

    void changeCursorPos(vec2<int> pos){
      vec2<int> v = pos - currentPos;
      iomovepos(v.x, v.y);
      currentPos = pos;
    }

    vec2<int> getPos(){
      return currentPos;
    }

    void overwriteAtLine(string str, int y_offset){
      int y = y_offset - currentPos.y;
      iomoveupdown_r(y);
      cout << "\x1b[K\r" << flush << str << flush;
      iomoveupdown(-y);
      int x = currentPos.x - (str.length());
      iomoverightleft(x);
    }
};


template<typename t> ostream &operator<<(ostream &os, vec2<t> v){
  os << "x: " << v.x << " y: " << v.y;
  return os;
}

template<typename t> void fillVector(vector<t> &vec, t defaultvar){
  for(int i = 0; i < vec.size(); i++)
    vec.operator[](i) = defaultvar;
}

class ms{
  private:
    vec2<int> spaceSize;
    size_t spacelen, totalBombs;
    vector<bool> bombPositions;
    vector<char> nums;

    static int getPos(vec2<int> p, vec2<int> spaceSize){
      return (p.y * spaceSize.x) + p.x;
    }

    static vec2<int> getPos(int p, vec2<int> spaceSize){
      return vec2<int>{p%spaceSize.x, (int)floor((double)p/spaceSize.x)};
    }

    template<typename t, typename cast> static void dumpVectorCout(vector<t> *vec, vec2<int> size){
      for(int n_y = 0; n_y < size.y; n_y++){
        for(int n_x = 0; n_x < size.x; n_x++)
          cout << (cast)vec->operator[](ms::getPos(vec2<int>{n_x, n_y}, size)) << " ";

        cout << endl;
      }
    }

    void randomizeBomb(int bombs){
      totalBombs = bombs;
      fillVector(bombPositions, false);
      srand(time(0));
      for(int i = 0; i < bombs; i++){
	      double randnum = (double)rand() / RAND_MAX;
	      long place = (int)round(randnum*spacelen);
	      if(bombPositions[place])
	        i--;
	      else
	        bombPositions[place] = true;
      }

      if(debug_showbomb)
        for(int y_i = 0; y_i < spaceSize.y; y_i++){
          for(int x_i = 0; x_i < spaceSize.x; x_i++)
            if(bombPositions[getPos(vec2<int>(x_i, y_i), spaceSize)])
              cout << "B";
            else
              cout << ".";

          cout << endl;
        }
    }

    void updateNums(){
      fillVector<char>(nums, '\0');
      int vecs_offset[] = {1,0,-1,0};
      vec2<int> vec2_offset_init{-1, -1};
      for(int n = 0; n < spacelen; n++){
        vec2<int> currentPos = getPos(n, spaceSize), newPos = currentPos + vec2_offset_init;
	
        if(bombPositions[getPos(currentPos, spaceSize)])
          continue;

        for(int off_i = 0; off_i < 4; off_i++){
          vec2<int> currentOff{vecs_offset[off_i], -vecs_offset[3-off_i]};
          for(int xy_i = 0; xy_i < 2; xy_i++){
            newPos = newPos + currentOff;
            int pos = getPos(newPos, spaceSize);
            if(isinbounds(newPos, spaceSize)){
              if(bombPositions[pos]){
                nums[getPos(currentPos, spaceSize)] += 1;
              }
            }
          }
        }
      }

      if(debug_shownums)
        for(int y_i = 0; y_i < spaceSize.y; y_i++){
          for(int x_i = 0; x_i < spaceSize.x; x_i++)
            cout << (int)nums[getPos(vec2<int>{x_i, y_i}, spaceSize)];

          cout << endl;
        }
    }

    //can be used recursively
    void updatePlayspace(vec2<int> cursor, vector<bool> *updateLines, vector<char> *blocks){
      if(isinbounds(cursor, spaceSize)){
        int currentPosint = getPos(cursor, spaceSize);
        
        if(!bombPositions[currentPosint]){
          char beforestate = blocks->operator[](currentPosint);
          blocks->operator[](currentPosint) = state_opened;
          updateLines->operator[](cursor.y) = true;

          char currentnum = nums[currentPosint];
          if(currentnum <= 0 && beforestate == state_closed){
            int vecs_offset[] = {1,0,-1,0};
            vec2<int> vec2_offset_init(-1, -1), currentOffset = cursor + vec2_offset_init;
            for(int off_i = 0; off_i < 4; off_i++){
              vec2<int> curroff{vecs_offset[off_i], -vecs_offset[3-off_i]};
              
              for(int off_i1 = 0; off_i1 < 2; off_i1++){
                currentOffset = currentOffset + curroff;
                updatePlayspace(currentOffset, updateLines, blocks);
              }
            }
          }
        }
      }
    }

    void updateLines(vector<bool> *linestoupdate, vector<char> *blockstates, cursorHandler *ch){
      for(int line_i = 0; line_i < linestoupdate->size(); line_i++)
        if(linestoupdate->operator[](line_i)){
          string newline;
          int offset = getPos(vec2<int>{0, line_i}, spaceSize);
          for(int str_i = 0; str_i < spaceSize.x; str_i++)
            switch(blockstates->operator[](str_i+offset)){
              case state_closed:{
                newline += char_state_closed;
                break;
              }

              case state_flagged:{
                newline += char_state_flagged;
                break;
              }

              case state_opened:{
                if(bombPositions[str_i+offset])
                  newline += char_state_bomb;
                else{
                  char currentNum = nums[getPos(vec2<int>(str_i, line_i), spaceSize)];
                  newline += (currentNum > 0)? to_string((int)currentNum): char_state_opened_null;
                }

                break;
              }
            }

          ch->overwriteAtLine(newline, line_i);
          linestoupdate->operator[](line_i) = false;
        }
    }

    void gui_updateFlags(int flagcount, cursorHandler *ch){
      string flag = "Flags: " + to_string(flagcount) + "/" + to_string(totalBombs);
      ch->overwriteAtLine(flag, -offset_scoreboard_to_playspace);
    }

    void gui_updateEventStr(string str, cursorHandler *ch){
      ch->overwriteAtLine(str, -offset_playspace_to_eventbar);
    }

  public:
    ms(vec2<int> size = minesweeper_spacesize_default, int bombs = -1){
      spacelen = size.x*size.y;
      spaceSize = size;
      bombPositions.resize(spacelen);
      nums.resize(spacelen);

      newPlayspace(bombs);
    }

    void newPlayspace(int bombs = -1){
      randomizeBomb(bombs < 0? roundl(spacelen*bombratio): bombs);
      updateNums();
    }

    void play(){
      cursorHandler ch{vec2<int>{}};
      vec2<int> currentCursor(0,0);
      vector<char> blockstate;
      vector<bool> updateLine;
      updateLine.resize(spaceSize.y);
      blockstate.resize(spacelen);
      fillVector(blockstate, (char)state_closed);
      char gamestate = 0;
      long currentCorrectFlags = 0, currentFlags = 0;
      size_t flagcount = 0, correctFlagCount = 0;
      int key;


      //make some space for playing space
      for(int i = 0; i < (spaceSize.y + offset_playspace_to_eventbar); i++)
        cout << endl;

      ch.changeOffset(vec2<int>{0, -spaceSize.y});
      
      fillVector(updateLine, true);
      updateLines(&updateLine, &blockstate, &ch);
      gui_updateFlags(flagcount, &ch);

      bool keepPlaying = true;
      while(keepPlaying && (key = _getch()) != EOF){
        int n_temp = 0;
        switch(key){
          case keypress_left:
            n_temp = currentCursor.x - 1;
            currentCursor.x = constraint(0, spaceSize.x, n_temp);
            break;
          
          case keypress_right:
            n_temp = currentCursor.x + 1;
            currentCursor.x = constraint(0, spaceSize.x, n_temp);
            break;
          
          case keypress_up:
            n_temp = currentCursor.y - 1;
            currentCursor.y = constraint(0, spaceSize.y, n_temp);
            break;

          case keypress_down:
            n_temp = currentCursor.y + 1;
            currentCursor.y = constraint(0, spaceSize.y, n_temp);
            break;
          
          case keypress_flag:{
            int currentPos = getPos(currentCursor, spaceSize);
            switch(blockstate[currentPos]){
              case state_opened:
                break;
              
              case state_closed:{
                //game ui editing is in here for now
                if(flagcount < totalBombs){
                  flagcount++;
                  blockstate[currentPos] = state_flagged;
                  updateLine[currentCursor.y] = true;

                  gui_updateFlags(flagcount, &ch);
                  if(bombPositions[currentPos]){
                    correctFlagCount++;
                    if(correctFlagCount >= totalBombs)
                      gamestate = gamestate_gamewin;
                  }
                }

                break;
              }

              case state_flagged:{
                flagcount--;
                blockstate[currentPos] = state_closed;
                updateLine[currentCursor.y] = true;

                gui_updateFlags(flagcount, &ch);
                if(bombPositions[currentPos])
                  correctFlagCount--;
                
                break;
              }
            }

            break;
          }

          case keypress_quit:{
            gamestate = gamestate_quitting;
            break;
          }

          //for now confirm just open the block
          case keypress_confirm:{
            int currentPos = getPos(currentCursor, spaceSize);
            if(blockstate[currentPos] != state_flagged){
              if(!bombPositions[currentPos])
                updatePlayspace(currentCursor, &updateLine, &blockstate);
              else
                gamestate = gamestate_gameover;

            }

            break;
          }
        }

        //do update lines
        ch.changeCursorPos(currentCursor);
        updateLines(&updateLine, &blockstate, &ch);

        //gamestate checking
        switch(gamestate){
          case gamestate_quitting:{
            keepPlaying = false;
            gui_updateEventStr("Quitting...", &ch);
            break;
          }

          case gamestate_gameover:{
            keepPlaying = false;
            gui_updateEventStr("Game over, try again.", &ch);
            for(int bombs_i = 0; bombs_i < bombPositions.size(); bombs_i++)
              if(bombPositions[bombs_i])
                blockstate[bombs_i] = state_opened;
              
            fillVector(updateLine, true);
            updateLines(&updateLine, &blockstate, &ch);
            break;
          }

          case gamestate_gamewin:{
            keepPlaying = false;
            gui_updateEventStr("You win!", &ch);
            break;
          }
        }
      }
    }
};


int main(){
  ms mine{};
  mine.play();
}
