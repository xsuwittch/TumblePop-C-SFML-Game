#include <iostream>
#include <fstream>
#include <cmath>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <SFML/Window.hpp>

using namespace sf;
using namespace std;

int screen_x = 1136;
int screen_y = 896;

char getTile(char **lvl, int row, int col, int height, int width)
{
    if (row < 0 || row >= height || col < 0 || col >= width)
        return ' ';
    return lvl[row][col];
}

bool isSolidTile(char t)
{
    return (t == '#' || t == '-' || t == '/' || t == '\\');
}

bool overlapsSolid(char **lvl, float x, float y, int w, int h, int cell_size)
{
    int left = (int)(x) / cell_size;
    int right = (int)(x + w - 1) / cell_size;
    int top = (int)(y) / cell_size;
    int bottom = (int)(y + h - 1) / cell_size;

    for (int r = top; r <= bottom; ++r)
    {
        for (int c = left; c <= right; ++c)
        {
            if (getTile(lvl, r, c, h, w) != ' ' && isSolidTile(getTile(lvl, r, c, h, w)))
                return true;
        }
    }
    return false;
}

void findValidSpawn(char **lvl, int &spawnRow, int &spawnCol, int type, int height, int width)
{
    // If current cell is already valid, keep it
    if (spawnRow >= 0 && spawnRow < height && spawnCol >= 0 && spawnCol < width)
    {
        char cur = getTile(lvl, spawnRow, spawnCol, height, width);
        if (cur == ' ')
        {
            if (type == 1 || type == 3)
            {
                // ground enemy: require solid beneath
                if (isSolidTile(getTile(lvl, spawnRow + 1, spawnCol, height, width)))
                    return;
            }
            else
            {
                // flying/invisible: empty cell ok
                return;
            }
        }
    }

    // Search in expanding radius for a valid cell
    int maxRadius = max(height, width);
    for (int r = 0; r <= maxRadius; r++)
    {
        for (int dc = -r; dc <= r; dc++)
        {
            int dr = r - abs(dc);
            for (int sign = -1; sign <= 1; sign += 2)
            {
                int nr = spawnRow + dr * sign;
                int nc = spawnCol + dc;
                if (nr < 0 || nr >= height || nc < 0 || nc >= width)
                    continue;
                char cur = getTile(lvl, nr, nc, height, width);
                if (cur != ' ')
                    continue;
                if (type == 1 || type == 3)
                {
                    if (isSolidTile(getTile(lvl, nr + 1, nc, height, width)))
                    {
                        spawnRow = nr;
                        spawnCol = nc;
                        return;
                    }
                }
                else
                {
                    spawnRow = nr;
                    spawnCol = nc;
                    return;
                }
            }
        }
    }

    // fallback: clamp
    spawnRow = max(0, min(spawnRow, height - 1));
    spawnCol = max(0, min(spawnCol, width - 1));
}

void display_level(RenderWindow &window, char **lvl, Texture &bgTex, Sprite &bgSprite, Texture &blockTexture, Sprite &blockSprite, Sprite &oneWaySprite, Sprite &slopeSprite, Sprite &slopeBotSprite, const int height, const int width, const int cell_size, int selectedLevel)
{
    window.draw(bgSprite);

    for (int i = 0; i < height; i += 1)
    {
        for (int j = 0; j < width; j += 1)
        {
            if (lvl[i][j] == '#')
            {
                blockSprite.setPosition(j * cell_size, i * cell_size);
                window.draw(blockSprite);
            }
            else if (lvl[i][j] == '-')
            {
                oneWaySprite.setPosition(j * cell_size, i * cell_size);
                window.draw(oneWaySprite);
            }
            else if (lvl[i][j] == '/')
            {
                slopeSprite.setPosition(j * cell_size, i * cell_size);
                window.draw(slopeSprite);
            }
            else if (lvl[i][j] == '\\')
            {
                slopeBotSprite.setPosition(j * cell_size, i * cell_size);
                window.draw(slopeBotSprite);
            }
        }
    }
}

bool enemy_horizontal_collision(char **lvl, float enemyX, float enemyY,
                                const int cell_size, int enemyWidth, int enemyHeight,
                                bool movingRight, float speed, int height, int width)
{
    float offset_x = enemyX;

    if (movingRight)
    {
        offset_x += speed;

        // Check right side of enemy
        char right_top = getTile(lvl, (int)(enemyY) / cell_size, (int)(offset_x + enemyWidth) / cell_size, height, width);
        char right_mid = getTile(lvl, (int)(enemyY + enemyHeight / 2) / cell_size, (int)(offset_x + enemyWidth) / cell_size, height, width);
        char right_bottom = getTile(lvl, (int)(enemyY + enemyHeight - 1) / cell_size, (int)(offset_x + enemyWidth) / cell_size, height, width);

        if (right_top == '#' || right_mid == '#' || right_bottom == '#' ||
            right_top == '\\' || right_mid == '\\' || right_bottom == '\\' ||
            right_top == '/' || right_mid == '/' || right_bottom == '/')
        {
            return true; // Hit wall, change direction
        }
    }
    else // Moving left
    {
        offset_x -= speed;

        // Check left side of enemy
        char left_top = getTile(lvl, (int)(enemyY) / cell_size, (int)(offset_x) / cell_size, height, width);
        char left_mid = getTile(lvl, (int)(enemyY + enemyHeight / 2) / cell_size, (int)(offset_x) / cell_size, height, width);
        char left_bottom = getTile(lvl, (int)(enemyY + enemyHeight - 1) / cell_size, (int)(offset_x) / cell_size, height, width);

        if (left_top == '#' || left_mid == '#' || left_bottom == '#' ||
            left_top == '\\' || left_mid == '\\' || left_bottom == '\\' ||
            left_top == '/' || left_mid == '/' || left_bottom == '/')
        {
            return true; // Hit wall, change direction
        }
    }

    return false; // No collision
}

bool end_of_platform(char **lvl, float &enemyX, float &enemyY,
                     float &velocityY, const int cell_size,
                     int enemyWidth, int enemyHeight,
                     const float gravity, int height, int width)
{
    velocityY += gravity;
    float offset_y = enemyY + velocityY;

    // Check ground below enemy
    char bottom_left = getTile(lvl, (int)(offset_y + enemyHeight) / cell_size, (int)(enemyX) / cell_size, height, width);
    char bottom_mid = getTile(lvl, (int)(offset_y + enemyHeight) / cell_size, (int)(enemyX + enemyWidth / 2) / cell_size, height, width);
    char bottom_right = getTile(lvl, (int)(offset_y + enemyHeight) / cell_size, (int)(enemyX + enemyWidth) / cell_size, height, width);

    bool hitGround = (bottom_left == '#' || bottom_mid == '#' || bottom_right == '#' ||
                      bottom_left == '-' || bottom_mid == '-' || bottom_right == '-' ||
                      bottom_left == '\\' || bottom_mid == '\\' || bottom_right == '\\' ||
                      bottom_left == '/' || bottom_mid == '/' || bottom_right == '/');

    if (!hitGround)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool enemy_vertical_collision(char **lvl, float &enemyX, float &enemyY,
                              float &velocityY, const int cell_size,
                              int enemyWidth, int enemyHeight,
                              const float gravity, int height, int width)
{
    velocityY += gravity;
    float offset_y = enemyY + velocityY;

    // Check ground below enemy
    char bottom_left = getTile(lvl, (int)(offset_y + enemyHeight) / cell_size, (int)(enemyX) / cell_size, height, width);
    char bottom_mid = getTile(lvl, (int)(offset_y + enemyHeight) / cell_size, (int)(enemyX + enemyWidth / 2) / cell_size, height, width);
    char bottom_right = getTile(lvl, (int)(offset_y + enemyHeight) / cell_size, (int)(enemyX + enemyWidth) / cell_size, height, width);

    bool hitGround = (bottom_left == '#' || bottom_mid == '#' || bottom_right == '#' ||
                      bottom_left == '-' || bottom_mid == '-' || bottom_right == '-' ||
                      bottom_left == '\\' || bottom_mid == '\\' || bottom_right == '\\' ||
                      bottom_left == '/' || bottom_mid == '/' || bottom_right == '/');

    if (hitGround && velocityY > 0)
    {
        // Snap to ground
        velocityY = 0;
        enemyY = ((int)(offset_y + enemyHeight) / cell_size) * cell_size - enemyHeight;
        return true; // On ground
    }
    else
    {
        // Keep falling
        enemyY = offset_y;
        return false; // In air
    }
}

void player_gravity(char **lvl, float &offset_y, float &velocityY, bool &onGround,
                    const float &gravity, float &terminal_Velocity,
                    float &player_x, float &player_y,
                    const int cell_size, int &Pheight, int &Pwidth,
                    bool dropDown, float &dropCooldown, bool &victoryAnimation)
{
    velocityY += gravity;
    if (velocityY >= terminal_Velocity)
        velocityY = terminal_Velocity;

    offset_y = player_y + velocityY;

    if (!victoryAnimation)
    {
        if (velocityY < 0)
        {
            char top_left = lvl[(int)(offset_y) / cell_size][(int)(player_x) / cell_size];
            char top_mid = lvl[(int)(offset_y) / cell_size][(int)(player_x + Pwidth / 2) / cell_size];
            char top_right = lvl[(int)(offset_y) / cell_size][(int)(player_x + Pwidth) / cell_size];

            if (top_left == '#' || top_mid == '#' || top_right == '#' ||
                top_left == '\\' || top_mid == '\\' || top_right == '\\' ||
                top_left == '/' || top_mid == '/' || top_right == '/')
            {
                velocityY = 0;
                player_y = ((int)(offset_y) / cell_size + 1) * cell_size;
                return;
            }
        }

        if (velocityY > 0)
        {
            char bottom_left = lvl[(int)(offset_y + Pheight) / cell_size][(int)(player_x) / cell_size];
            char bottom_mid = lvl[(int)(offset_y + Pheight) / cell_size][(int)(player_x + Pwidth / 2) / cell_size];
            char bottom_right = lvl[(int)(offset_y + Pheight) / cell_size][(int)(player_x + Pwidth) / cell_size];

            bool hitSolidBlock = (bottom_left == '#' || bottom_mid == '#' || bottom_right == '#' ||
                                bottom_left == '\\' || bottom_mid == '\\' || bottom_right == '\\' ||
                                bottom_left == '/' || bottom_mid == '/' || bottom_right == '/');
            bool hitOneWay = (bottom_left == '-' || bottom_mid == '-' || bottom_right == '-');

            if (dropDown && hitOneWay && !hitSolidBlock)
            {
                onGround = false;
                player_y = offset_y;
                dropCooldown = 0.1f;
            }
            else if (dropCooldown > 0 && hitOneWay && !hitSolidBlock)
            {
                onGround = false;
                player_y = offset_y;
            }
            else if (hitSolidBlock)
            {
                onGround = true;
                velocityY = 0;
                player_y = ((int)(offset_y + Pheight) / cell_size) * cell_size - Pheight;
                dropCooldown = 0;
            }
            else if (hitOneWay && !dropDown && dropCooldown <= 0)
            {
                onGround = true;
                velocityY = 0;
                player_y = ((int)(offset_y + Pheight) / cell_size) * cell_size - Pheight;
            }
            else
            {
                onGround = false;
                player_y = offset_y;
            }
        }
        else
        {
            onGround = false;
            player_y = offset_y;
        }
    }
}

void player_horizontal_collision(char **lvl, float &player_x, float &player_y, const int cell_size, int &Pheight, int &Pwidth, float speed, bool movingLeft, bool movingRight, bool &victoryAnimation)
{
    float offset_x = player_x;

    if (!victoryAnimation)
    {
        if (movingLeft)
        {
            offset_x -= speed;

            char left_top = lvl[(int)(player_y) / cell_size][(int)(offset_x) / cell_size];
            char left_mid = lvl[(int)(player_y + Pheight / 2) / cell_size][(int)(offset_x) / cell_size];
            char left_bottom = lvl[(int)(player_y + Pheight - 1) / cell_size][(int)(offset_x) / cell_size];

            if (left_top == '#' || left_mid == '#' || left_bottom == '#' ||
                left_top == '\\' || left_mid == '\\' || left_bottom == '\\' ||
                left_top == '/' || left_mid == '/' || left_bottom == '/')
            {
                player_x = (((int)(offset_x) / cell_size) + 1) * cell_size;
            }
            else
            {
                player_x = offset_x;
            }
        }

        if (movingRight)
        {
            offset_x = player_x + speed;

            char right_top = lvl[(int)(player_y) / cell_size][(int)(offset_x + Pwidth) / cell_size];
            char right_mid = lvl[(int)(player_y + Pheight / 2) / cell_size][(int)(offset_x + Pwidth) / cell_size];
            char right_bottom = lvl[(int)(player_y + Pheight - 1) / cell_size][(int)(offset_x + Pwidth) / cell_size];

            if (right_top == '#' || right_mid == '#' || right_bottom == '#' ||
                right_top == '\\' || right_mid == '\\' || right_bottom == '\\' ||
                right_top == '/' || right_mid == '/' || right_bottom == '/')
            {
                player_x = ((int)(offset_x + Pwidth) / cell_size) * cell_size - Pwidth;
            }
            else
            {
                player_x = offset_x;
            }
        }
    }
}

void updateGhost(char **lvl, float &ghostX, float &ghostY, bool &goingRight,
                 float &ghostSpeed, float &velocityY, const int cell_size,
                 Sprite &ghostSpr, Texture &ghostLeftTex, Texture &ghostRightTex)
{
    // Move left/right but avoid entering solid tiles
    float nextX = ghostX + (goingRight ? ghostSpeed : -ghostSpeed);

    // check if moving to nextX would overlap a solid tile (ghost size 64x64)
    if (lvl != nullptr && overlapsSolid(lvl, nextX, ghostY, 64, 64, cell_size))
    {
        // hit a wall/block, turn around
        goingRight = !goingRight;
    }
    else
    {
        ghostX = nextX;
    }

    // Change direction at screen boundaries as a fallback
    if (ghostX > 850)
        goingRight = false;
    if (ghostX < 250)
        goingRight = true;

    // Update sprite texture and position
    if (goingRight)
        ghostSpr.setTexture(ghostRightTex);
    else
        ghostSpr.setTexture(ghostLeftTex);

    ghostSpr.setPosition(ghostX, ghostY);
}

void drawGhost(RenderWindow &window, Sprite &ghostSpr)
{
    window.draw(ghostSpr);
}

void updateskel(char **lvl, float &skelX, float &skelY, bool &skelgoingRight,
                float &skelSpeed, float &velocityY, const int cell_size,
                Sprite &skelSpr, Texture &skelLeftTex, Texture &skelRightTex,
                float playerX, float playerY, float &jumpCooldown, float &walkTimer, int height, int width)
{
    // Update vertical movement (gravity / ground snapping)
    bool onGround = enemy_vertical_collision(lvl, skelX, skelY, velocityY, cell_size, 64, 64, 1.0f, height, width);

    // Predict next horizontal position
    float nextX = skelX + (skelgoingRight ? skelSpeed : -skelSpeed);

    // Determine foot check coordinates (one tile ahead beneath the enemy)
    int footRow = (int)((skelY + 64) / cell_size);
    int footColAhead = (int)((skelgoingRight ? (nextX + 64) : nextX) / cell_size);

    // If no ground ahead, or horizontal collision ahead, turn around
    bool groundAhead = (getTile(lvl, footRow, footColAhead, height, width) == '#' || getTile(lvl, footRow, footColAhead, height, width) == '-' ||
                        getTile(lvl, footRow, footColAhead, height, width) == '/' || getTile(lvl, footRow, footColAhead, height, width) == '\\');

    bool willHitWall = enemy_horizontal_collision(lvl, skelX, skelY, cell_size, 64, 64, skelgoingRight, skelSpeed, height, width);

    if (!groundAhead || willHitWall)
    {
        skelgoingRight = !skelgoingRight;
        // recompute nextX after turning
        nextX = skelX + (skelgoingRight ? skelSpeed : -skelSpeed);
    }

    // If on ground, possibly jump up to a higher platform ahead if reachable.
    // Jump behavior is randomized unless player is above, in which case skeletons
    // will be more likely to try to reach the player.
    if (onGround)
    {
        // accumulate a short walk timer so skeletons don't try to jump immediately
        walkTimer += 1.0f / 60.0f;
        if (walkTimer > 2.0f)
            walkTimer = 2.0f; // clamp
        // decrement cooldown
        if (jumpCooldown > 0)
            jumpCooldown -= 1.0f / 60.0f;

        const float skelJumpStrength = -15.0f; // negative to move upward
        int headRow = (int)(skelY / cell_size);

        // decide whether to consider jumping (random chance or player-above trigger)
        bool considerJump = false;
        // if player is significantly above the skeleton and roughly horizontally near, prefer jumping
        if (playerY + 5 < skelY && fabs(playerX - skelX) < 300.0f) // player above
        {
            considerJump = true;
        }
        else
        {
            // 12% base chance to consider jumping
            if ((rand() % 100) < 12)
                considerJump = true;
        }

        // require a short walk (about 0.3s) before considering a jump
        if (considerJump && jumpCooldown <= 0.0f && walkTimer >= 0.3f)
        {
            // Check 1 or 2 tiles above the current foot row for a platform to jump onto
            for (int jumpTiles = 1; jumpTiles <= 2; ++jumpTiles)
            {
                int landingRow = (int)((skelY + 64) / cell_size) - jumpTiles; // footRow - jumpTiles
                int landingCol = footColAhead;

                if (landingRow < 0)
                    break;

                // If there is a solid tile at the landing row/col, consider jumping
                if (isSolidTile(getTile(lvl, landingRow, landingCol, height, width)))
                {
                    // Ensure vertical clearance: no solid tiles between current head and landing row
                    bool clear = true;
                    for (int r = landingRow + 1; r <= headRow - 1; ++r)
                    {
                        if (isSolidTile(getTile(lvl, r, landingCol, height, width)))
                        {
                            clear = false;
                            break;
                        }
                    }

                    // Also ensure we won't immediately hit a wall horizontally
                    if (clear && !willHitWall)
                    {
                        velocityY = skelJumpStrength; // initiate jump (will apply in next frames)
                        jumpCooldown = 0.8f;          // seconds until next allowed jump
                        walkTimer = 0.0f;             // reset walk timer after jump
                        break;
                    }
                }
            }
        }
    }

    // Move horizontally
    skelX = nextX;

    // Update sprite based on direction
    if (skelgoingRight)
        skelSpr.setTexture(skelRightTex);
    else
        skelSpr.setTexture(skelLeftTex);

    skelSpr.setPosition(skelX, skelY);
}


void drawskel(RenderWindow &window, Sprite &skelSpr)
{
    window.draw(skelSpr);
}

void updateinvisibleman(float &invisVelocityY, char **lvl, const int cell_size, float playerY, float playerX, float &invisX, float &invisY, bool &invisGoingRight, float &invisSpeed,
                        Sprite &invisSpr, Texture &invisLeftTex, Texture &invisRightTex, bool &isInvisible,
                        float &invisibleTimer, float &invisibleDuration, float &nextDisappearTime, bool &Disappearing, int &invisDisappearFrame, int height, int width)
{
    static int frameCounter = 0;
    if (isInvisible)
    {
        invisX = playerX + 40;
        invisY = playerY;
        if (invisibleTimer > invisibleDuration)
        {
            isInvisible = 0;
            invisibleTimer = 0;
        }
        else
        {
            invisibleTimer++;
        }
    }
    if (!isInvisible)
    {
        nextDisappearTime--;
    }
    if (nextDisappearTime == 0 && !Disappearing)
    {
        invisDisappearFrame = 0;
        Disappearing = 1;
        invisibleDuration = 60;
        nextDisappearTime = rand() % 600;
    }

    if (Disappearing)
    {

        frameCounter++;

        if (frameCounter >= 10)
        {
            invisDisappearFrame++;
            frameCounter = 0;
        }
        if (invisDisappearFrame >= 6)
        {
            frameCounter = 0;
            isInvisible = 1;
            Disappearing = 0;
            nextDisappearTime = rand() % 1000;
        }
    }

    if (!Disappearing && !isInvisible)
    {
        bool onGround = enemy_vertical_collision(lvl, invisX, invisY, invisVelocityY,
                                                 cell_size, 64, 64, 1.0f, height, width);

        if (onGround)
        {
            if (enemy_horizontal_collision(lvl, invisX, invisY, cell_size,
                                           64, 64, invisGoingRight, invisSpeed, height, width))
            {
                invisGoingRight = !invisGoingRight; // Turn around at wall
            }
            else
            {
                // Move if no collision
                if (invisGoingRight)
                    invisX += invisSpeed;
                else
                    invisX -= invisSpeed;
            }
        }
        if (invisGoingRight)
            invisSpr.setTexture(invisRightTex);
        else
            invisSpr.setTexture(invisLeftTex);

        invisSpr.setPosition(invisX, invisY);
    }
}


void drawinvisibleman(RenderWindow &window, Sprite &invisSpr, bool isInvisible, bool &Disappearing, int &invisDisappearFrame, Sprite Disappear_spr[])
{
    if (Disappearing)
    {
        Disappear_spr[invisDisappearFrame].setPosition(invisSpr.getPosition());
        window.draw(Disappear_spr[invisDisappearFrame]);
    }

    else if (!isInvisible)
    {
        window.draw(invisSpr);
    }
}

bool detectPlayer(float playerX, float playerY, float enemyX, float enemyY, bool &genovaGoingRight)
{

    if ((fabs(playerY - enemyY) < 50) && (fabs(playerX - enemyX) < 200))
    {
        if (enemyX > playerX)
        {
            if (genovaGoingRight)
            {
                genovaGoingRight = false;
            }
        }
        else
        {
            genovaGoingRight = true;
        }
        return true;
    }
    else
    {
        return false;
    }
}

void updateGenova(char **lvl, const int cell_size, float playerX, float playerY,
                  float &genovaX, float &genovaY,
                  bool &genovaGoingRight, float &genovaSpeed,
                  Sprite &genovaSpr,
                  Texture &genovaLeftTex, Texture &genovaRightTex,
                  int vacuumFrame,
                  bool &isAttacking,
                  int &attackTimer,
                  int &fireballCooldown, int height, int width)
{
    if (!isAttacking)
    {
        // Predict next horizontal position
        float nextX = genovaX + (genovaGoingRight ? genovaSpeed : -genovaSpeed);

        // foot check: tile below the enemy one cell ahead
        int footRow = (int)((genovaY + 64) / cell_size);
        int footColAhead = (int)((genovaGoingRight ? (nextX + 64) : nextX) / cell_size);

        bool groundAhead = isSolidTile(getTile(lvl, footRow, footColAhead, height, width));

        // Check horizontal collision ahead
        bool willHitWall = enemy_horizontal_collision(lvl, genovaX, genovaY,
                                                      cell_size, 64, 64,
                                                      genovaGoingRight, genovaSpeed, height, width);

        if (!groundAhead || willHitWall)
        {
            genovaGoingRight = !genovaGoingRight;
        }
        else
        {
            genovaX = nextX;
        }
    }

    if (genovaGoingRight)
        genovaSpr.setTexture(genovaRightTex);
    else
        genovaSpr.setTexture(genovaLeftTex);

    genovaSpr.setPosition(genovaX, genovaY);

    // Only attack if the player is within detection range AND is in front of the Genova
    bool playerDetected = (fabs(playerY - genovaY) < 50) && (fabs(playerX - genovaX) < 200);
    bool playerIsInFront = (genovaGoingRight && playerX > genovaX) || (!genovaGoingRight && playerX < genovaX);

    const int windupFrames = 24; // 0.4s at 60fps
    if (playerDetected && playerIsInFront && fireballCooldown <= 0)
    {
        if (!isAttacking || attackTimer <= 0)
        {
            isAttacking = true;
            // set wind-up timer (frames until fireball spawn)
            attackTimer = windupFrames;
        }
    }
    else
    {
        if (isAttacking)
        {
            // allow drawGenova to finish wind-up; if wind-up already done, cancel attack
            if (attackTimer <= 0)
                isAttacking = false;
        }
    }
}

bool hitPlayer(float X, float Y, float playerX, float playerY)
{
    if (abs(X - playerX) < 20 && abs(Y - playerY) < 30)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool playerDodged(float X, float Y, float playerX, float playerY, bool facingRight)
{
    if (facingRight)
    {
        if (X > playerX)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        if (X < playerX)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
}

void drawGenova(RenderWindow &window,
                Sprite &genovaSpr,
                bool &isAttacking,
                bool facingRight,
                int &attackTimer, float playerX, float playerY, int &fireballCooldown, bool &fireballSpawned,
                Sprite genova_sprite[], Sprite fire_sprite[], float genovaX, float genovaY, int vacuumframe, bool &fireballActive, float &fireballX, float &fireballY, bool &fireballRight, float &fireballSpeed, bool &fireballHit,
                int &lifeCount, float &damageCooldown, bool vacuum)
{

    // Position animation frames at this Genova's coordinates so frames draw at correct spot
    for (int f = 0; f < 6; ++f)
    {
        genova_sprite[f].setPosition(genovaX, genovaY);
    }

    const int windupFrames = 24; // frames of wind-up animation (0.4s)
    if (isAttacking)
    {
        int frameOffset = facingRight ? 3 : 0;

        // If timer > 0, show progress through the 3-frame attack animation
        if (attackTimer > 0)
        {
            // compute a 0..2 frame index based on progress
            int progress = windupFrames - attackTimer;
            int attackFrameIdx = (progress * 3) / max(1, windupFrames);
            attackFrameIdx = max(0, min(2, attackFrameIdx));
            window.draw(genova_sprite[attackFrameIdx + frameOffset]);

            // decrement timer
            attackTimer -= 1;

            // when timer reaches zero, spawn the fireball
            if (attackTimer <= 0)
            {
                if ((!fireballSpawned) && fireballCooldown <= 0)
                {
                    fireballActive = true;
                    fireballX = genovaX;
                    fireballY = genovaY + 10;
                    fireballRight = facingRight;
                    // set cooldown to 4 seconds (assuming 60 FPS)
                    fireballCooldown = 240;
                    fireballSpawned = true;
                }

                // attack finished
                isAttacking = false;
            }
        }
        else
        {
            // fallback: draw first attack frame if timer is unexpectedly zero
            window.draw(genova_sprite[0 + frameOffset]);
            isAttacking = false;
        }
    }
    else
    {
        window.draw(genovaSpr);
        fireballSpawned = false;
    }
    if (fireballActive)
    {
        if (fireballRight)
        {
            fireballX += fireballSpeed;
        }

        else
        {
            fireballX -= fireballSpeed;
        }

        fire_sprite[vacuumframe / 5].setPosition(fireballX, fireballY);
        window.draw(fire_sprite[vacuumframe / 5]);

        // If the fireball hits the player, mark the hit and deactivate the fireball.
        if (hitPlayer(fireballX, fireballY, playerX, playerY))
        {
            // Apply damage immediately if not vacuuming and cooldown allows
            if (!vacuum && damageCooldown <= 0.0f)
            {
                if (lifeCount > -1)
                {
                    lifeCount -= 1;
                }
                damageCooldown = 2.0f;
            }

            fireballActive = false;
            fireballSpawned = false;
            fireballHit = true;
        }
        else if (playerDodged(fireballX, fireballY, playerX, playerY, facingRight))
        {
            // If player dodged, simply deactivate the fireball without marking a hit
            fireballActive = false;
            fireballSpawned = false;
        }
    }
}


void level1(char **lvl, Texture &bgTex, Sprite &bgSprite, Texture &blockTexture, Sprite &blockSprite, Texture &oneWayTexture, Sprite &oneWaySprite)
{
    // Load textures with error checking
    bgTex.loadFromFile("Data/bg.png");
    bgSprite.setTexture(bgTex);
    bgSprite.setPosition(0, 0);

    blockTexture.loadFromFile("Data/block1.png");
    blockSprite.setTexture(blockTexture);

    oneWayTexture.loadFromFile("Data/block1.png");
    oneWaySprite.setTexture(oneWayTexture);

    for (int i = 0; i < 18; i++)
        lvl[0][i] = '#';
    for (int i = 0; i < 13; i++)
        lvl[i][17] = '#';
    for (int i = 0; i < 13; i++)
        lvl[i][0] = '#';
    for (int i = 0; i < 18; i++)
        lvl[13][i] = '#';

    for (int i = 3; i < 15; i++)
        lvl[3][i] = '-';
    for (int i = 3; i < 15; i++)
        lvl[11][i] = '-';

    for (int i = 3; i < 15; i++)
    {
        if (!((i == 8) || (i == 9)))
            lvl[7][i] = '-';
    }
    for (int i = 0; i < 17; i++)
    {
        if (i < 5 || i > 12)
            lvl[5][i] = '-';
    }
    for (int i = 0; i < 17; i++)
    {
        if (i < 5 || i > 12)
            lvl[9][i] = '-';
    }

    lvl[3][8] = '#';
    lvl[3][9] = '#';
    lvl[4][8] = '#';
    lvl[4][9] = '#';
    lvl[5][8] = '#';
    lvl[5][9] = '#';
    lvl[5][7] = '#';
    lvl[5][10] = '#';
    lvl[6][9] = '#';
    lvl[6][8] = '#';
    lvl[7][9] = '#';
    lvl[7][8] = '#';
    lvl[8][9] = '#';
    lvl[8][8] = '#';
    lvl[10][8] = '#';
    lvl[10][9] = '#';
    lvl[9][8] = '#';
    lvl[9][9] = '#';
    lvl[9][7] = '#';
    lvl[9][10] = '#';
    lvl[6][7] = '#';
    lvl[6][10] = '#';
    lvl[7][7] = '#';
    lvl[7][10] = '#';
    lvl[8][7] = '#';
    lvl[8][10] = '#';
}

void level2(char **lvl, Texture &bgTex, Sprite &bgSprite, Texture &blockTexture, Sprite &blockSprite, Texture &slopeTexture, Sprite &slopeSprite, Texture &slopeBotTexture, Sprite &slopeBotSprite, Texture &oneWayTexture, Sprite &oneWaySprite)
{
    // Load textures with error checking
    bgTex.loadFromFile("Data/bg2.png");
    bgSprite.setTexture(bgTex);
    bgSprite.setPosition(0, 0);

    blockTexture.loadFromFile("Data/block2.png");
    blockSprite.setTexture(blockTexture);

    slopeTexture.loadFromFile("Data/slope.png");
    slopeSprite.setTexture(slopeTexture);

    slopeBotTexture.loadFromFile("Data/slope_bottom.png");
    slopeBotSprite.setTexture(slopeBotTexture);

    oneWayTexture.loadFromFile("Data/block2.png");
    oneWaySprite.setTexture(oneWayTexture);

    for (int i = 0; i < 18; i++) // for upper line
    {

        lvl[0][i] = '#';
    }

    for (int i = 0; i < 13; i++) // for right line
    {

        lvl[i][17] = '#';
    }
    for (int i = 0; i < 13; i++) // for left line
    {

        lvl[i][0] = '#';
    }
    for (int i = 0; i < 18; i++) // for lower line
    {

        lvl[13][i] = '#';
    }

    for (int i = 0; i < 17; i++)
    {
        if (!((i >= 3 && i <= 5)) && !(i > 15))
        {
            lvl[3][i] = '-';
        }
    }
    for (int i = 0; i < 17; i++)
    {
        if ((i >= 1 && i <= 4) || (i >= 11 && i <= 12) || (i >= 15))
        {
            lvl[11][i] = '-';
        }
    }

    for (int i = 0; i < 15; i++)
    {
        if ((i >= 1 && i <= 2) || (i >= 10 && i <= 13))
        {
            lvl[7][i] = '-';
        }
    }
    for (int i = 0; i < 17; i++)
    {
        if ((i == 1) || (i >= 8 && i <= 10) || (i >= 13))
        {
            lvl[5][i] = '-';
        }
    }
    for (int i = 0; i < 17; i++)
    {
        if ((i >= 1 && i <= 3) || (i >= 12 && i <= 14) || (i == 16))
        {
            lvl[9][i] = '-';
        }
    }

    for (int i = 3; i < 17; i++)
    {
        int j = i + 1;
        if (j < 12 && i < 12)
        {
            lvl[i][i] = '/';
            lvl[j][i] = '\\';
            if (j < 11)
            {
                lvl[j + 1][i] = '\\';
            }
        }
    }
}

void startLevel(int &selectedLevel, int height, int width, int &lifeCount, float &player_x, float &player_y, float &velocityY, bool &onGround, float &damageCooldown, float &dropCooldown,
                char **lvl, Texture &bgTex, Sprite &bgSprite, Texture &blockTexture, Sprite &blockSprite, Texture &oneWayTexture, Sprite &oneWaySprite,
                Texture &slopeTexture, Sprite &slopeSprite, Texture &slopeBotTexture, Sprite &slopeBotSprite, bool &spacePressed, Music &lvlMusic,
                int enemyTypes[], float enemyX[], float enemyY[], bool enemyDisappeared[], bool enemySucked[], bool enemyThrown[], float enemyVelocityY[], bool enemyGoingRight[], float enemyThrowVelocityX[], float enemyThrowVelocityY[], 
                int MAX_ENEMIES, const int cell_size)
{
    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
            lvl[i][j] = ' ';

    if (selectedLevel == 1)
    {
        player_x = 200;
        player_y = 150;

        level1(lvl, bgTex, bgSprite, blockTexture, blockSprite, oneWayTexture, oneWaySprite);
    }
    else if (selectedLevel == 2)
    {
        player_x = 400;
        player_y = 150;

        level2(lvl, bgTex, bgSprite, blockTexture, blockSprite, slopeTexture, slopeSprite, slopeBotTexture, slopeBotSprite, oneWayTexture, oneWaySprite);
    }

    lifeCount = 3;
    velocityY = 0;
    onGround = false;
    damageCooldown = 0.0f;
    dropCooldown = 0.0f;

    lvlMusic.play();
    lvlMusic.setLoop(true);
    spacePressed = true;

    // ===== FIXED VERSION: Properly declare and initialize arrays =====
    const int defaultSpawnCols[10] = {3, 2, 10, 14, 9, 16, 7, 12, 15, 9};
    const int defaultSpawnRows[10] = {6, 3, 9, 3, 11, 12, 7, 6, 8, 12};

    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        enemyTypes[i] = i % 4;
        enemyDisappeared[i] = false;
        enemySucked[i] = false;
        enemyThrown[i] = false;
        enemyVelocityY[i] = 0;
        enemyGoingRight[i] = true;
        enemyThrowVelocityX[i] = 0;
        enemyThrowVelocityY[i] = 0;

        int spawnCol = defaultSpawnCols[i];
        int spawnRow = defaultSpawnRows[i];
        
        findValidSpawn(lvl, spawnRow, spawnCol, enemyTypes[i], height, width);
        
        enemyX[i] = spawnCol * cell_size;
        enemyY[i] = spawnRow * cell_size;
    }

    lvlMusic.play();
    lvlMusic.setLoop(true);
    spacePressed = true;
}

int main()
{
    RenderWindow window(VideoMode(screen_x, screen_y), "Tumble-POP", Style::Resize);
    window.setVerticalSyncEnabled(true);
    window.setFramerateLimit(60);

    const int cell_size = 64;
    const int height = 14;
    const int width = 18;
    char **lvl;

    int gameState = 0;
    int selectedLevel = 1;

    float player_x = 500;
    float player_y = 150;

    sf::Color darkBlue(200, 0, 10, 255);
    sf::Color lightWhite(0, 0, 0);
    Texture bgmenutex;
    Sprite bgmenusprite;

    if (!bgmenutex.loadFromFile("Data/tumblebg.jpg")) // bg load
        cout << "Failed to load tumblebg.png" << endl;

    bgmenusprite.setTexture(bgmenutex);
    bgmenusprite.setPosition(0, 0);

    Texture logoTex;
    Sprite logoSprite;

    if (!logoTex.loadFromFile("Data/logo.png")) // logo load
        cout << "Failed to load logo.png" << endl;

    Texture bgTex;
    Sprite bgSprite;
    Texture blockTexture;
    Sprite blockSprite;
    Texture slopeTexture;
    Sprite slopeSprite;
    Texture Disappear[6];
    Sprite Disappear_spr[6];
    Texture slopeBotTexture;
    Sprite slopeBotSprite;

    Texture rainbow_tex[16];
    Sprite rainbow_sprite[16];
    Texture heartTex[3];
    Sprite heartSpr[3];
    Texture walk_tex[8];
    Sprite walk_sprite[8];
    Texture victoryTex[4];
    Sprite victorySpr[4];
    Texture playerLogoTex;
    Sprite playerLogoSpr;
    Texture playerNumTex;
    Sprite playerNumSpr;
    const int backCapLevel1 = 3;
    const int backCapLevel2 = 5;
    int backpackLevel1[backCapLevel1]; // 0 = Ghost, 1 = Skeleton, 2 = Invisible Man, 3 = Genova
    int backpackLevel2[backCapLevel2];
    int backCountLevel1 = 0;
    int backCountLevel2 = 0;
    bool isInvisible = false;
    float invisibleTimer = 0;
    float invisibleDuration = 0;
    bool Disappearing = 0;
    int invisDisappearFrame = 0;
    srand(time(NULL));
    float nextDisappearTime = rand() % 1000;
    for (int i = 0; i < 6; i++)
    {
        string file = "Data/invisible/getinvisible_" + to_string(i) + ".png";
        if (!Disappear[i].loadFromFile(file))
        {
            cout << " Error loading disappearing textures! " << "\n";
        }
        Disappear_spr[i].setTexture(Disappear[i]);
        Disappear_spr[i].setScale(1.8, 1.8);
    }

    for (int i = 0; i < 4; i++)
    {
        string filename = "Data/vacuum/" + to_string(i + 1) + ".png";
        if (!rainbow_tex[i].loadFromFile(filename))
            cout << "Failed to load " << filename << endl;
        rainbow_sprite[i].setTexture(rainbow_tex[i]);
        rainbow_sprite[i].setPosition(player_x, player_y);
    }

    for (int i = 4; i < 16; i++)
    {
        string filename = "Data/vacuum/" + to_string(i + 1) + ".png";
        if (!rainbow_tex[i].loadFromFile(filename))
            cout << "Failed to load " << filename << endl;
        rainbow_sprite[i].setTexture(rainbow_tex[i]);
        rainbow_sprite[i].setPosition(player_x, player_y);
    }

    for (int i = 0; i < 3; i++)
    {
        heartTex[i].loadFromFile("Data/heart.png");
    }

    for (int i = 0; i < 3; i++)
    {
        heartSpr[i].setTexture(heartTex[i]);
    }

    for (int i = 0; i < 8; i++)
    {
        string filename = "Data/walk/" + to_string(i) + ".png";
        if (!walk_tex[i].loadFromFile(filename))
            cout << "Failed to load " << filename << endl;
        walk_sprite[i].setTexture(walk_tex[i]);
        walk_sprite[i].setPosition(player_x, player_y);
    }

    for (int i = 0; i < 4; i++)
    {
        string filename = "Data/victory/" + to_string(i) + ".png";
        if (!victoryTex[i].loadFromFile(filename))
            cout << "Failed to load " << filename << endl;
        victorySpr[i].setTexture(victoryTex[i]);
        victorySpr[i].setPosition(player_x, player_y);
    }

    playerLogoTex.loadFromFile("Data/player_logo.png");
    playerLogoSpr.setTexture(playerLogoTex);
    playerLogoSpr.setScale(1.5, 1.5);
    playerLogoSpr.setPosition(8, 8);

    playerNumTex.loadFromFile("Data/player_num.png");
    playerNumSpr.setTexture(playerNumTex);
    playerNumSpr.setScale(2.5, 2.5);
    playerNumSpr.setPosition(64, 12);

    Music lvlMusic;
    if (!lvlMusic.openFromFile("Data/mus.ogg"))
        cout << "Failed to load mus.ogg" << endl;
    lvlMusic.setVolume(20);

    float speed = 5;
    float dropTimer = 0.0f;
    const float dropDuration = 0.15f;
    const float jumpStrength = -17;
    const float gravity = 1;

    float victoryTimer = 0.0f;
    bool victoryAnimation = false;

    bool isJumping = false;
    bool up_collide = false;
    bool left_collide = false;
    bool right_collide = false;

    Texture PlayerTextureRight;
    Texture PlayerTextureLeft;
    Texture PlayerTextureUp;
    Texture PlayerTextureDown;
    Sprite PlayerSprite;
    Texture PlayerTextureJumpRight;
    Texture PlayerTextureJumpLeft;
    Texture oneWayTexture;
    Sprite oneWaySprite;

    // Genova Data
    Texture genovaLeftTex;
    Texture genovaRightTex;
    Sprite genovaSpr;

    bool genovaGoingRight = true;
    float genovaSpeed = 1.5f;
    bool genovaIsAttacking = false;
    int genovaAttackFrame = 0;
    int fireballCooldown = 0;
    bool fireballSpawned = false;
    genovaSpr.setScale(1.8, 1.8);
    genovaLeftTex.loadFromFile("Data/Genova/genova_left.png");
    genovaRightTex.loadFromFile("Data/Genova/genova_right.png");

    // Start facing right
    genovaSpr.setTexture(genovaRightTex);

    // fire ball

    bool fireballActive = false;
    float fireballX = 0;
    float fireballY = 0;
    float fireballSpeed = 4.0f;
    bool fireballRight = true;

    Texture fire_tex[4];   // for vacuum
    Sprite fire_sprite[4]; // for vacuum

    //Enemy spawn
    const int MAX_ENEMIES = 10;
    int enemyTypes[MAX_ENEMIES];
    float enemyX[MAX_ENEMIES];
    float enemyY[MAX_ENEMIES];
    float enemyVelocityY[MAX_ENEMIES];
   // per-enemy speed (used for genova movement so one Genova's attack doesn't stop others)
    float enemySpeedArr[MAX_ENEMIES];
    // per-enemy jump cooldown (frames until next allowed jump)
    float enemyJumpCooldownArr[MAX_ENEMIES];
    bool enemyGoingRight[MAX_ENEMIES];
    bool enemyDisappeared[MAX_ENEMIES];
    bool enemySucked[MAX_ENEMIES];
    bool enemyThrown[MAX_ENEMIES];
    float enemyThrowVelocityX[MAX_ENEMIES];
    float enemyThrowVelocityY[MAX_ENEMIES];
    // per-enemy short walk timer so skeletons walk a bit before attempting to jump
    float enemyWalkTimerArr[MAX_ENEMIES];
    // per-enemy previous position and stuck-frame counter to detect stuck enemies
    float enemyPrevX[MAX_ENEMIES];
    float enemyPrevY[MAX_ENEMIES];
    int enemyStuckFrames[MAX_ENEMIES];
    int activeEnemyCount = 0;
    // Per-enemy Genova attack / fireball state
    bool genovaIsAttackingArr[MAX_ENEMIES];
    int genovaAttackFrameArr[MAX_ENEMIES];
    bool fireballActiveArr[MAX_ENEMIES];
    float fireballXArr[MAX_ENEMIES];
    float fireballYArr[MAX_ENEMIES];
    bool fireballRightArr[MAX_ENEMIES];
    int fireballCooldownArr[MAX_ENEMIES];
    bool fireballSpawnedArr[MAX_ENEMIES];
    bool fireballHitArr[MAX_ENEMIES];

    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        enemyTypes[i] = i % 4; // Cycle through 0,1,2,3
        // by default make all slots active
        enemyDisappeared[i] = false;
        enemySucked[i] = false;
        enemySucked[i] = false;
        enemyThrown[i] = false;
        enemyVelocityY[i] = 0;
        enemyGoingRight[i] = true;
        enemyThrowVelocityX[i] = 0;
        enemyThrowVelocityY[i] = 0;
        enemyWalkTimerArr[i] = 0.0f;
        enemySpeedArr[i] = 1.5f; // default speed for moving enemies (Genova uses this)
        enemyJumpCooldownArr[i] = 0;
        // Make skeleton at index 1 less likely to jump immediately (reduce glitching)
        if (i == 1)
        {
            enemyJumpCooldownArr[i] = 1.2f; // 1.2s cooldown before first allowed jump
            enemyWalkTimerArr[i] = 0.3f;    // require ~0.3s walk before jump
        }
        genovaIsAttackingArr[i] = false;
        genovaAttackFrameArr[i] = 0;
        fireballActiveArr[i] = false;
        fireballXArr[i] = 0;
        fireballYArr[i] = 0;
        fireballRightArr[i] = true;
        fireballCooldownArr[i] = 0;
        fireballSpawnedArr[i] = false;
        fireballHitArr[i] = false;

        // Hardcoded spawn positions (col,row) for each enemy slot.
        // Spread skeletons: index 1 = top, index 5 = right, index 9 = bottom
        const int defaultSpawnCols[MAX_ENEMIES] = {3, 2, 10, 14, 9, 16, 7, 12, 15, 9};
        const int defaultSpawnRows[MAX_ENEMIES] = {6, 3, 9, 3, 11, 12, 7, 6, 8, 12};

        int spawnCol = defaultSpawnCols[i];
        int spawnRow = defaultSpawnRows[i];
        enemyX[i] = spawnCol * cell_size;
        enemyY[i] = spawnRow * cell_size;
        // initialize prev pos and stuck counter
        enemyPrevX[i] = enemyX[i];
        enemyPrevY[i] = enemyY[i];
        enemyStuckFrames[i] = 0;
        // (No level validation here because `lvl` is allocated later.)
    }

    for (int i = 0; i < 4; i++)
    {
        string filename = "Data/Genova/fireball/" + to_string(i + 1) + ".png";
        fire_tex[i].loadFromFile(filename);
    }

    for (int i = 0; i < 4; i++)
    {

        fire_sprite[i].setTexture(fire_tex[i]);
        fire_sprite[i].setPosition(enemyX[1], enemyY[1]);
        fire_sprite[i].setPosition(enemyX[5], enemyY[5]);
        fire_sprite[i].setScale(1.8, 1.8);
    }

    Texture genova_tex[6]; // for genova attack
    Sprite genova_sprite[6];

    for (int i = 0; i < 6; i++)
    {
        string filename = "Data/Genova/Genova_throw/" + to_string(i + 1) + ".png";
        genova_tex[i].loadFromFile(filename);
    }

    for (int i = 0; i < 6; i++)
    {

        genova_sprite[i].setTexture(genova_tex[i]);
        genova_sprite[i].setPosition(enemyX[1], enemyY[1]);
        genova_sprite[i].setPosition(enemyX[5], enemyY[5]);
        genova_sprite[i].setScale(1.8, 1.8);
    }

    // Ghost data
    Texture ghostLeftTex;
    Texture ghostRightTex;
    Sprite ghostSpr;

    bool goingRight = true;
    float ghostSpeed = 1.8f;

    ghostSpr.setScale(1.8, 1.8);
    ghostLeftTex.loadFromFile("Data/Ghost/ghost_left.png");
    ghostRightTex.loadFromFile("Data/Ghost/ghost_right.png");

    // Start facing right
    ghostSpr.setTexture(ghostRightTex);

    // skeleton data
    Texture skelLeftTex;
    Texture skelRightTex;
    Sprite skelSpr;

    float skelX = 500;
    float skelY = 765;
    bool skelgoingRight = true;
    float skelSpeed = 2.0f;

    skelSpr.setScale(1.8, 1.8);
    skelLeftTex.loadFromFile("Data/Skeleton/skeleton_left.png");
    skelRightTex.loadFromFile("Data/Skeleton/skeleton_right.png");

    // Start facing right
    skelSpr.setTexture(ghostRightTex);

    // invisible man data
    Texture invisLeftTex;
    Texture invisRightTex;
    Sprite invisSpr;

    bool invisGoingRight = true;
    float invisSpeed = 2.0f;

    invisSpr.setScale(1.8, 1.8);
    invisLeftTex.loadFromFile("Data/invisible/iman_left.png");
    invisRightTex.loadFromFile("Data/invisible/iman_right.png");

    // Start facing right
    invisSpr.setTexture(ghostRightTex);

    int walkframe = 0;
    int vacuumframe = 0;
    int victoryFrame = 0;
    bool onGround = false;

    int lifeCount = 3;

    float offset_x = 0;
    float offset_y = 0;
    float velocityY = 0;

    float terminal_Velocity = 20;
    float dropCooldown = 0.0f;
    float damageCooldown = 0.0f;

    int PlayerHeight = 64;
    int PlayerWidth = 68;

    bool up_button = false;

    char top_left = '\0';
    char top_right = '\0';
    char top_mid = '\0';

    char left_mid = '\0';
    char right_mid = '\0';

    char bottom_left = '\0';
    char bottom_right = '\0';
    char bottom_mid = '\0';

    char bottom_left_down = '\0';
    char bottom_right_down = '\0';
    char bottom_mid_down = '\0';

    char top_right_up = '\0';
    char top_mid_up = '\0';
    char top_left_up = '\0';

    bool dropDown = false;
    bool facingRight = true;

    float throwVelocityX = 15.0f;
    float throwVelocityY = 15.0f;

    bool ghostDisappear = false;
    bool skelDisappear = false;
    bool invisDisappear = false;
    bool genovaDisappear = false;

    bool ghostThrown = false;
    bool skelThrown = false;
    bool invisThrown = false;
    bool genovaThrown = false;

    float ghostThrowVelocityX = 0.0f;
    float skelThrowVelocityX = 0.0f;
    float invisThrowVelocityX = 0.0f;
    float genovaThrowVelocityX = 0.0f;

    float ghostThrowVelocityY = 0.0f;
    float skelThrowVelocityY = 0.0f;
    float invisThrowVelocityY = 0.0f;
    float genovaThrowVelocityY = 0.0f;

    bool ghostSucked = false;
    bool skelSucked = false;
    bool invisSucked = false;
    bool genovaSucked = false;

    if (!PlayerTextureJumpRight.loadFromFile("Data/jump_right.png"))
        cout << "Failed to load jump_right.png" << endl;
    if (!PlayerTextureJumpLeft.loadFromFile("Data/jump_left.png"))
        cout << "Failed to load jump_left.png" << endl;
    if (!PlayerTextureRight.loadFromFile("Data/player_right.png"))
        cout << "Failed to load player_right.png" << endl;
    if (!PlayerTextureLeft.loadFromFile("Data/player_left.png"))
        cout << "Failed to load player_left.png" << endl;
    if (!PlayerTextureUp.loadFromFile("Data/player_up.png"))
        cout << "Failed to load player_up.png" << endl;
    if (!PlayerTextureDown.loadFromFile("Data/player_down.png"))

    PlayerSprite.setTexture(PlayerTextureRight);
    PlayerSprite.setScale(2, 2);
    PlayerSprite.setPosition(player_x, player_y);

    lvl = new char *[height];
    for (int i = 0; i < height; i += 1)
    {
        lvl[i] = new char[width];
        for (int j = 0; j < width; j++)
            lvl[i][j] = ' ';
    }

    Event ev;

    bool spacePressed = false;
    bool escapePressed = false;

    Font font;
    bool fontLoaded = font.loadFromFile("Data/arialbd.ttf");
    if (!fontLoaded)
        cout << "Failed to load font" << endl;

    Text titleText;
    titleText.setFont(font);
    Text level1Text;
    level1Text.setFont(font);
    Text level2Text;
    level2Text.setFont(font);
    Text instructText;
    instructText.setFont(font);

    while (window.isOpen())
    {
        window.clear(Color::Black);

        // Handle window events in both states
        while (window.pollEvent(ev))
        {
            if (ev.type == Event::Closed)
                window.close();
        }

        // ===== MENU SCREEN =====
        if (gameState == 0)
        {
            window.draw(bgmenusprite);

           /*RectangleShape titleBox(Vector2f(600, 150));
            titleBox.setPosition(screen_x / 2 - 300, 100);
            titleBox.setFillColor(lightWhite);
            window.draw(titleBox);*/

            if (fontLoaded) // code for text load and show
            {
                // titleText.setString("TUMBLE-POP");
                // titleText.setCharacterSize(60);
                // titleText.setFillColor(Color::White);
                logoSprite.setTexture(logoTex);
                logoSprite.setScale(0.5, 0.5);
                window.draw(logoSprite);
                logoSprite.setPosition(screen_x / 2 - 250, 200);
            }

            RectangleShape level1Box(Vector2f(400, 80));
            level1Box.setPosition(screen_x / 2 - 200, 350);
            if (selectedLevel == 1)
                level1Box.setFillColor(darkBlue);

            else
                level1Box.setFillColor(Color::Green);
            window.draw(level1Box);

            if (fontLoaded)
            {
                level1Text.setString("LEVEL 1");
                level1Text.setCharacterSize(40);
                level1Text.setFillColor(Color::Black);
                level1Text.setPosition(level1Box.getPosition().x + 120, level1Box.getPosition().y + 15);
                window.draw(level1Text);
            }

            RectangleShape level2Box(Vector2f(400, 80));
            level2Box.setPosition(screen_x / 2 - 200, 470);
            if (selectedLevel == 2)
                level2Box.setFillColor(darkBlue);
            else
                level2Box.setFillColor(Color::Green);
            window.draw(level2Box);

            if (fontLoaded)
            {
                level2Text.setString("LEVEL 2");
                level2Text.setCharacterSize(40);
                level2Text.setFillColor(Color::Black);
                level2Text.setPosition(level2Box.getPosition().x + 120, level2Box.getPosition().y + 15);
                window.draw(level2Text);
            }

            if (fontLoaded)
            {
                string instruct = "UP/DOWN to Select | SPACE to Play | ESC to Exit\n\n"
                                  "LEFT/RIGHT for Movement\nC for Jump\n"
                                  "W/S for Up/Down Vacuum\n"
                                  "Q for Bulk Throw\nE for Single Throw\n"
                                  "S + C for Drop";
                instructText.setString(instruct);
                instructText.setCharacterSize(20);
                instructText.setFillColor(Color::Black);
                instructText.setPosition(screen_x / 2 - 225, 600);
                window.draw(instructText);
            }

            static bool upPressed = false;
            if (Keyboard::isKeyPressed(Keyboard::Up) && !upPressed)
            {
                if (selectedLevel == 2)
                    selectedLevel = 1;
                upPressed = true;
            }
            if (!Keyboard::isKeyPressed(Keyboard::Up))
                upPressed = false;

            static bool downPressed = false;
            if (Keyboard::isKeyPressed(Keyboard::Down) && !downPressed)
            {
                if (selectedLevel == 1)
                    selectedLevel = 2;
                downPressed = true;
            }
            if (!Keyboard::isKeyPressed(Keyboard::Down))
                downPressed = false;

            if (Keyboard::isKeyPressed(Keyboard::Escape) && !escapePressed)
            {
                window.close();
                escapePressed = true;
            }
            if (!Keyboard::isKeyPressed(Keyboard::Escape))
                escapePressed = false;

            if (Keyboard::isKeyPressed(Keyboard::Space) && !spacePressed)
            {
                gameState = 1;
                backCountLevel1 = 0;
                backCountLevel2 = 0;
                startLevel(selectedLevel, height, width, lifeCount, player_x, player_y, velocityY, onGround, damageCooldown, dropCooldown,
                        lvl, bgTex, bgSprite, blockTexture, blockSprite, oneWayTexture, oneWaySprite,
                        slopeTexture, slopeSprite, slopeBotTexture, slopeBotSprite, spacePressed, lvlMusic,
                        enemyTypes, enemyX, enemyY, enemyDisappeared, enemySucked, enemyThrown, 
                        enemyVelocityY, enemyGoingRight, enemyThrowVelocityX, enemyThrowVelocityY, 
                        MAX_ENEMIES, cell_size);
            }
            if (!Keyboard::isKeyPressed(Keyboard::Space))
                spacePressed = false;
        }

        // ===== PLAYING SCREEN =====
        else if (gameState == 1)
        {
            if (Keyboard::isKeyPressed(Keyboard::Escape) && !escapePressed)
            {
                gameState = 0;
                lvlMusic.stop();
                escapePressed = true;
            }
            if (!Keyboard::isKeyPressed(Keyboard::Escape))
                escapePressed = false;

            float suctionSpeed = 5.0f;
            int heartDistance = 64;
            int heartPosition = -54;

            // Create proximity bools for each enemy
            bool ghostActive = !ghostDisappear && !ghostSucked;
            bool skelActive = !skelDisappear && !skelSucked;
            bool invisActive = !invisDisappear && !invisSucked;
            bool genovaActive = !genovaDisappear && !genovaSucked;
            // Check collision with all active enemies
            bool activeMonsterCollision = false;
            for (int i = 0; i < MAX_ENEMIES; i++)
            {
                if (enemyDisappeared[i] || enemySucked[i])
                    continue;

                bool xCollision = enemyX[i] >= (player_x - 32) && enemyX[i] <= (player_x + 32);
                bool yCollision = enemyY[i] >= (player_y - 32) && enemyY[i] <= (player_y + 32);

                if (xCollision && yCollision)
                {
                    activeMonsterCollision = true;
                    break;
                }
            }

            bool suctionRangeGhostX;
            bool suctionRangeSkelX;
            bool suctionRangeInvisX;
            bool suctionRangeGenovaX;

            bool suctionRangeGhostY;
            bool suctionRangeSkelY;
            bool suctionRangeInvisY;
            bool suctionRangeGenovaY;

            vacuumframe++;
            if (vacuumframe >= 20)
                vacuumframe = 0;

            victoryFrame++;
            if (victoryFrame >= 20)
                victoryFrame = 0;

            walkframe++;
            if (walkframe >= 32)
                walkframe = 0;

            for (int i = 0; i < 6; i++)
            {
                genova_sprite[i].setPosition(enemyX[1], enemyY[1]);
                genova_sprite[i].setPosition(enemyX[5], enemyY[5]);
            }

            bool movingLeft = Keyboard::isKeyPressed(Keyboard::Left);
            bool movingRight = Keyboard::isKeyPressed(Keyboard::Right);
            bool pressingJump = Keyboard::isKeyPressed(Keyboard::C);
            bool pressingUp = Keyboard::isKeyPressed(Keyboard::W);
            bool pressingDown = Keyboard::isKeyPressed(Keyboard::S);
            bool vacuum = Keyboard::isKeyPressed(Keyboard::Space);
            bool bulkThrow = Keyboard::isKeyPressed(Keyboard::Q);

            static bool singleThrowPressed = false;
            bool singleThrow = false;

            // E press
            if (Keyboard::isKeyPressed(Keyboard::E) && !singleThrowPressed)
            {
                singleThrow = true;
                singleThrowPressed = true;
            }
            if (!Keyboard::isKeyPressed(Keyboard::E))
            {
                singleThrowPressed = false;
            }

            if (!onGround)
            {
                if (facingRight)
                    PlayerSprite.setTexture(PlayerTextureJumpRight);
                else
                    PlayerSprite.setTexture(PlayerTextureJumpLeft);
            }
            else if (movingLeft)
            {
                facingRight = false;
                PlayerSprite.setTexture(PlayerTextureLeft);
            }
            else if (movingRight)
            {
                facingRight = true;
                PlayerSprite.setTexture(PlayerTextureRight);
            }
            else if (pressingUp)
            {
                facingRight = false;
                PlayerSprite.setTexture(PlayerTextureUp);
            }
            else if (pressingDown)
            {
                facingRight = false;
                PlayerSprite.setTexture(PlayerTextureDown);
            }
            else
            {
                if (facingRight)
                    PlayerSprite.setTexture(PlayerTextureRight);
                else
                    PlayerSprite.setTexture(PlayerTextureLeft);
            }

            if (pressingDown && pressingJump && onGround)
                dropTimer = dropDuration;
            else if (pressingJump && onGround)
                velocityY = jumpStrength;

            if (dropTimer > 0)
                dropTimer -= 1.0f / 60.0f;

            dropDown = dropTimer > 0;

            if (dropCooldown > 0)
                dropCooldown -= 1.0f / 60.0f;

            player_horizontal_collision(lvl, player_x, player_y, cell_size, PlayerHeight, PlayerWidth, speed, movingLeft, movingRight, victoryAnimation);
            display_level(window, lvl, bgTex, bgSprite, blockTexture, blockSprite, oneWaySprite, slopeSprite, slopeBotSprite, height, width, cell_size, selectedLevel);
            player_gravity(lvl, offset_y, velocityY, onGround, gravity, terminal_Velocity, player_x, player_y, cell_size, PlayerHeight, PlayerWidth, dropDown, dropCooldown, victoryAnimation);

            if (!victoryAnimation)
            {
                if (!movingLeft && !movingRight)
                {
                    window.draw(PlayerSprite);
                }
                PlayerSprite.setPosition(player_x, player_y);

                if (movingLeft && onGround)
                {
                    int frameIndex = (walkframe / 20) % 4; // Frames 0-3
                    walk_sprite[frameIndex].setPosition(player_x, player_y);
                    walk_sprite[frameIndex].setScale(2, 2);
                    window.draw(walk_sprite[frameIndex]);
                }
                else if (movingRight && onGround)
                {
                    int frameIndex = 4 + ((walkframe / 20) % 4); // Frames 4-7
                    walk_sprite[frameIndex].setPosition(player_x, player_y);
                    walk_sprite[frameIndex].setScale(2, 2);
                    window.draw(walk_sprite[frameIndex]);
                }
                else
                {
                    window.draw(PlayerSprite);
                }
            }

            // Check left, mid and right points underneath player sprite
            // If player sprite is on slope blocks, increase player axes to give slide effect
            if (selectedLevel == 2)
            {
                if (onGround)
                {
                    int slopeGridY = (int)(player_y + PlayerHeight) / cell_size;

                    int slopeGridXLeft = (int)(player_x) / cell_size;
                    int slopeGridXMid = (int)(player_x + PlayerWidth / 2) / cell_size;
                    int slopeGridXRight = (int)(player_x + PlayerWidth - 1) / cell_size;

                    char tileLeft = ' ', tileMid = ' ', tileRight = ' ';

                    if (slopeGridY >= 0 && slopeGridY < height)
                    {
                        if (slopeGridXLeft >= 0 && slopeGridXLeft < width)
                            tileLeft = lvl[slopeGridY][slopeGridXLeft];
                        if (slopeGridXMid >= 0 && slopeGridXMid < width)
                            tileMid = lvl[slopeGridY][slopeGridXMid];
                        if (slopeGridXRight >= 0 && slopeGridXRight < width)
                            tileRight = lvl[slopeGridY][slopeGridXRight];
                    }

                    if (tileLeft == '/' || tileMid == '/' || tileRight == '/')
                    {
                        player_x += 2.0f;
                        player_y += 2.0f;
                    }
                    else if (tileLeft == '\\' || tileMid == '\\' || tileRight == '\\')
                    {
                        player_x -= 2.0f;
                        player_y += 2.0f;
                    }
                }
            }
            
            for (int i = 0; i < MAX_ENEMIES; i++)
            {
                if (enemyDisappeared[i] || enemySucked[i] || enemyThrown[i])
                    continue;

                if (enemyTypes[i] == 0)
                { // Ghost
                    updateGhost(lvl, enemyX[i], enemyY[i], enemyGoingRight[i],
                                ghostSpeed, enemyVelocityY[i], cell_size,
                                ghostSpr, ghostLeftTex, ghostRightTex);
                }
                else if (enemyTypes[i] == 1)
                { // Skeleton
                    updateskel(lvl, enemyX[i], enemyY[i], enemyGoingRight[i],
                               enemySpeedArr[i], enemyVelocityY[i], cell_size,
                               skelSpr, skelLeftTex, skelRightTex,
                               player_x, player_y, enemyJumpCooldownArr[i], enemyWalkTimerArr[i], height, width);
                }
                else if (enemyTypes[i] == 2)
                { // Invisible Man
                    updateinvisibleman(enemyVelocityY[i], lvl, cell_size, player_y,
                                       player_x, enemyX[i], enemyY[i], enemyGoingRight[i],
                                       invisSpeed, invisSpr, invisLeftTex, invisRightTex,
                                       isInvisible, invisibleTimer, invisibleDuration,
                                       nextDisappearTime, Disappearing, invisDisappearFrame, height, width);
                }
                else if (enemyTypes[i] == 3)
                { // Genova
                    // Use per-enemy attack state arrays so Genovas don't interfere
                    updateGenova(lvl, cell_size, player_x, player_y, enemyX[i], enemyY[i],
                                 enemyGoingRight[i], enemySpeedArr[i], genovaSpr,
                                 genovaLeftTex, genovaRightTex, vacuumframe,
                                 genovaIsAttackingArr[i], genovaAttackFrameArr[i], fireballCooldownArr[i], height, width);
                }
            }

            // After updates, ensure no enemy is stuck inside solid tiles; if so, relocate them
            for (int i = 0; i < MAX_ENEMIES; ++i)
            {
                if (enemyDisappeared[i] || enemySucked[i])
                    continue;
                if (overlapsSolid(lvl, enemyX[i], enemyY[i], 64, 64, cell_size))
                {
                    // Avoid relocating enemies that are currently moving upward (jumping),
                    // since they may temporarily intersect tiles while ascending.
                    if (enemyVelocityY[i] < 0.0f)
                        continue;
                    int spawnCol = (int)(enemyX[i] / cell_size);
                    int spawnRow = (int)(enemyY[i] / cell_size);
                    findValidSpawn(lvl, spawnRow, spawnCol, enemyTypes[i], height, width);
                    enemyX[i] = spawnCol * cell_size;
                    enemyY[i] = spawnRow * cell_size;
                }

                // Level 2: disable slot 0 (user requested special handling for level 2)
                if (selectedLevel == 2)
                {
                    enemyDisappeared[0] = true;
                }

                // Stuck detection: if an enemy hasn't moved for a while, relocate (especially ghosts)
                for (int i = 0; i < MAX_ENEMIES; ++i)
                {
                    if (enemyDisappeared[i] || enemySucked[i])
                        continue;
                    float dx = fabs(enemyX[i] - enemyPrevX[i]);
                    float dy = fabs(enemyY[i] - enemyPrevY[i]);
                    if (dx < 1.0f && dy < 1.0f)
                    {
                        enemyStuckFrames[i] += 1;
                    }
                    else
                    {
                        enemyStuckFrames[i] = 0;
                    }

                    // update previous position for next frame
                    enemyPrevX[i] = enemyX[i];
                    enemyPrevY[i] = enemyY[i];

                    // If stuck for >30 frames (~0.5s), relocate ghosts to a valid nearby spawn
                    if (enemyStuckFrames[i] > 30)
                    {
                        if (enemyTypes[i] == 0)
                        {
                            int sc = (int)(enemyX[i] / cell_size);
                            int sr = (int)(enemyY[i] / cell_size);
                            findValidSpawn(lvl, sr, sc, enemyTypes[i], height, width);
                            enemyX[i] = sc * cell_size;
                            enemyY[i] = sr * cell_size;
                        }
                        enemyStuckFrames[i] = 0;
                    }
                }
            }

            for (int i = 0; i < MAX_ENEMIES; i++)
            {
                if (enemyDisappeared[i])
                    continue;

                if (enemyTypes[i] == 0)
                { // Ghost
                    // Ensure correct facing per enemy
                    if (enemyGoingRight[i])
                        ghostSpr.setTexture(ghostRightTex);
                    else
                        ghostSpr.setTexture(ghostLeftTex);

                    ghostSpr.setPosition(enemyX[i], enemyY[i]);
                    drawGhost(window, ghostSpr);
                    // (debug index removed)
                }
                else if (enemyTypes[i] == 1)
                { // Skeleton
                    // Use per-enemy direction for texture
                    if (enemyGoingRight[i])
                        skelSpr.setTexture(skelRightTex);
                    else
                        skelSpr.setTexture(skelLeftTex);

                    skelSpr.setPosition(enemyX[i], enemyY[i]);
                    drawskel(window, skelSpr);
                    // (debug index removed)
                }
                else if (enemyTypes[i] == 2)
                { // Invisible Man
                    // Invisible man uses its own appearing logic; ensure texture matches direction
                    if (enemyGoingRight[i])
                        invisSpr.setTexture(invisRightTex);
                    else
                        invisSpr.setTexture(invisLeftTex);

                    invisSpr.setPosition(enemyX[i], enemyY[i]);
                    drawinvisibleman(window, invisSpr, isInvisible, Disappearing,
                                     invisDisappearFrame, Disappear_spr);
                }
                else if (enemyTypes[i] == 3)
                { // Genova
                    // Use per-enemy direction rather than the single global
                    bool facing = enemyGoingRight[i];
                    if (facing)
                        genovaSpr.setTexture(genovaRightTex);
                    else
                        genovaSpr.setTexture(genovaLeftTex);

                    genovaSpr.setPosition(enemyX[i], enemyY[i]);
                    // decrement per-enemy fireball cooldown (frames)
                    if (fireballCooldownArr[i] > 0)
                        fireballCooldownArr[i] -= 1;
                    // Draw and manage fireball using per-enemy state arrays
                    drawGenova(window, genovaSpr, genovaIsAttackingArr[i], facing,
                               genovaAttackFrameArr[i], player_x, player_y, fireballCooldownArr[i],
                               fireballSpawnedArr[i], genova_sprite, fire_sprite, enemyX[i],
                               enemyY[i], vacuumframe, fireballActiveArr[i], fireballXArr[i], fireballYArr[i],
                               fireballRightArr[i], fireballSpeed, fireballHitArr[i], lifeCount, damageCooldown, vacuum);
                }
            }

            if (vacuum && !victoryAnimation)
            {
                for (int i = 0; i < 4; i++) // loop for vacum animation 
                {
                    if (facingRight)
                    {
                        window.draw(rainbow_sprite[(vacuumframe / 5) + 4]);
                        rainbow_sprite[i + 4].setPosition(player_x + 60, player_y + 25);
                    }
                    else if (pressingUp)
                    {
                        window.draw(rainbow_sprite[(vacuumframe / 5) + 8]);
                        rainbow_sprite[i + 8].setPosition(player_x - 5, player_y - 53);
                    }
                    else if (pressingDown)
                    {
                        window.draw(rainbow_sprite[(vacuumframe / 5) + 12]);
                        rainbow_sprite[i + 12].setPosition(player_x - 2, player_y + 69);
                    }
                    else
                    {
                        window.draw(rainbow_sprite[vacuumframe / 5]);
                        rainbow_sprite[i].setPosition(player_x - 50, player_y + 25);
                    }
                }
                
                for (int i = 0; i < MAX_ENEMIES; i++)
                {
                    if (enemyDisappeared[i] || enemySucked[i])
                        continue;
                    
                    bool suctionRangeX;
                    bool suctionRangeY;

                    if (pressingUp || pressingDown)
                    {
                        suctionRangeX = enemyX[i] >= (player_x - 150) && enemyX[i] <= (player_x + 150);
                        suctionRangeY = enemyY[i] >= (player_y - 32) && enemyY[i] <= (player_y + 32);
                    }
                    else
                    {
                        suctionRangeX = enemyX[i] >= (player_x - 100) && enemyX[i] <= (player_x + 100);
                        suctionRangeY = enemyY[i] >= (player_y - 100) && enemyY[i] <= (player_y + 100);
                    }

                    if (!suctionRangeX || !suctionRangeY)
                        continue;

                    // If Genova is currently attacking, vacuum has no effect on it
                    if (enemyTypes[i] == 3 && genovaIsAttackingArr[i])
                        continue;

                    // Horizontal suction (left/right)
                    if (!pressingUp && !pressingDown && ((backCountLevel1 < 3 && selectedLevel == 1) || (backCountLevel2 < 5 && selectedLevel == 2)))
                    {
                        if (facingRight && ((backCountLevel1 < 3 && selectedLevel == 1) || (backCountLevel2 < 5 && selectedLevel == 2)))
                        {
                            if (enemyX[i] >= player_x && suctionRangeX && suctionRangeY)
                            {
                                enemyX[i] -= suctionSpeed;
                                if (abs(enemyX[i] - player_x) < suctionSpeed)
                                {
                                    if (selectedLevel == 1 && backCountLevel1 < backCapLevel1)
                                    {
                                        backpackLevel1[backCountLevel1++] = i;
                                        enemyDisappeared[i] = true;
                                        enemySucked[i] = true;
                                    }
                                    else if (selectedLevel == 2 && backCountLevel2 < backCapLevel2)
                                    {
                                        backpackLevel2[backCountLevel2++] = i;
                                        enemyDisappeared[i] = true;
                                        enemySucked[i] = true;
                                    }
                                }
                            }  
                        } 
                        else if (((backCountLevel1 < 3 && selectedLevel == 1) || (backCountLevel2 < 5 && selectedLevel == 2)))
                        {
                            enemyX[i] += suctionSpeed;
                            if (enemyX[i] <= player_x && suctionRangeX && suctionRangeY)
                            {
                                if (abs(enemyX[i] - player_x) < suctionSpeed)
                                {
                                    if (selectedLevel == 1 && backCountLevel1 < backCapLevel1)
                                    {
                                        backpackLevel1[backCountLevel1++] = i;
                                        enemyDisappeared[i] = true;
                                        enemySucked[i] = true;
                                    }
                                    else if (selectedLevel == 2 && backCountLevel2 < backCapLevel2)
                                    {
                                        backpackLevel2[backCountLevel2++] = i;
                                        enemyDisappeared[i] = true;
                                        enemySucked[i] = true;
                                    }
                                }
                            }
                        }
                    }

                    // Vertical suction (up/down)
                    else if (pressingUp && enemyY[i] <= player_y && ((backCountLevel1 < 3 && selectedLevel == 1) || (backCountLevel2 < 5 && selectedLevel == 2)))
                    {
                        // If Genova is attacking, vacuum has no effect on it
                        if (enemyTypes[i] == 3 && genovaIsAttackingArr[i])
                            continue;
                        enemyY[i] += suctionSpeed;
                        if (suctionRangeX && suctionRangeY)
                        {
                            if (abs(enemyY[i] - player_y) < suctionSpeed)
                            {
                                if (backCountLevel1 < backCapLevel1)
                                {
                                    backpackLevel1[backCountLevel1++] = i;
                                    enemyDisappeared[i] = true;
                                    enemySucked[i] = true;
                                }
                                else if (backCountLevel2 < backCapLevel2)
                                {
                                    backpackLevel2[backCountLevel2++] = i;
                                    enemyDisappeared[i] = true;
                                    enemySucked[i] = true;
                                }
                            }
                        }
                    }
                    else if (pressingDown && enemyY[i] >= player_y && ((backCountLevel1 < 3 && selectedLevel == 1) || (backCountLevel2 < 5 && selectedLevel == 2)))
                    {
                        // If Genova is attacking, vacuum has no effect on it
                        if (enemyTypes[i] == 3 && genovaIsAttackingArr[i])
                            continue;
                        enemyY[i] -= suctionSpeed;

                        if (suctionRangeX && suctionRangeY)
                        {
                            if (abs(enemyY[i] - player_y) < suctionSpeed)
                            {
                                if (backCountLevel1 < backCapLevel1)
                                {
                                    backpackLevel1[backCountLevel1++] = i;
                                    enemyDisappeared[i] = true;
                                    enemySucked[i] = true;
                                }
                                else if (backCountLevel2 < backCapLevel2)
                                {
                                    backpackLevel2[backCountLevel2++] = i;
                                    enemyDisappeared[i] = true;
                                    enemySucked[i] = true;
                                }
                            }
                        }
                    }
                }
            }

            if (singleThrow && !(pressingDown || pressingUp) && ((backCountLevel1 > 0 && selectedLevel == 1) || (backCountLevel2 > 0 && selectedLevel == 2)))
            {
                throwVelocityX = 15.0f;

                if (!facingRight && throwVelocityX > 0.0f)
                {
                    throwVelocityX = -throwVelocityX;
                }

                int enemyIdx;
                int monsterType;

                if (selectedLevel == 1)
                {
                    enemyIdx = backpackLevel1[--backCountLevel1];
                    monsterType = enemyTypes[enemyIdx];
                }
                else if (selectedLevel == 2)
                {
                    enemyIdx = backpackLevel2[--backCountLevel2];
                    monsterType = enemyTypes[enemyIdx];
                }

                enemyDisappeared[enemyIdx] = false;
                enemyThrown[enemyIdx] = true;
                enemyThrowVelocityX[enemyIdx] = throwVelocityX;
                enemyThrowVelocityY[enemyIdx] = 0;
                enemyX[enemyIdx] = player_x;
                enemyY[enemyIdx] = player_y;
                enemySucked[enemyIdx] = false;
            }

            if (singleThrow && (pressingDown || pressingUp) && ((backCountLevel1 > 0 && selectedLevel == 1) || (backCountLevel2 > 0 && selectedLevel == 2)))
            {
                throwVelocityY = 15.0f;

                if (!pressingDown && throwVelocityY > 0.0f)
                {
                    throwVelocityY = -throwVelocityY;
                }

                int enemyIdx;
                int monsterType;

                if (selectedLevel == 1)
                {
                    enemyIdx = backpackLevel1[--backCountLevel1];
                    monsterType = enemyTypes[enemyIdx];
                }
                else if (selectedLevel == 2)
                {
                    enemyIdx = backpackLevel2[--backCountLevel2];
                    monsterType = enemyTypes[enemyIdx];
                }

                enemyDisappeared[enemyIdx] = false;
                enemyThrown[enemyIdx] = true;
                enemyThrowVelocityY[enemyIdx] = throwVelocityY;
                enemyThrowVelocityX[enemyIdx] = 0;
                enemyX[enemyIdx] = player_x;
                enemyY[enemyIdx] = player_y;
                enemySucked[enemyIdx] = false;
            }

            if (bulkThrow && !(pressingDown || pressingUp) && ((backCountLevel1 > 0 && selectedLevel == 1) || (backCountLevel2 > 0 && selectedLevel == 2)))
            {
                throwVelocityX = 15.0f;

                // If sucked when facing left, make throw negative (ONLY if it was positive)
                if (!facingRight && throwVelocityX > 0.0f)
                {
                    throwVelocityX = -throwVelocityX;
                }

                int enemyIdx;
                int monsterType;

                if (selectedLevel == 1)
                {
                    enemyIdx = backpackLevel1[--backCountLevel1];
                    monsterType = enemyTypes[enemyIdx];
                }
                else if (selectedLevel == 2)
                {
                    enemyIdx = backpackLevel2[--backCountLevel2];
                    monsterType = enemyTypes[enemyIdx];
                }

                enemyDisappeared[enemyIdx] = false;
                enemyThrown[enemyIdx] = true;
                enemyThrowVelocityX[enemyIdx] = throwVelocityX;
                enemyThrowVelocityY[enemyIdx] = 0;
                enemyX[enemyIdx] = player_x;
                enemyY[enemyIdx] = player_y;
                enemySucked[enemyIdx] = false;
            }

            // Check if Q pressed and any monster in backpack
            else if (bulkThrow && (pressingDown || pressingUp) && ((backCountLevel1 > 0 && selectedLevel == 1) || (backCountLevel2 > 0 && selectedLevel == 2)))
            {
                throwVelocityY = 15.0f;

                if (!pressingDown && throwVelocityY > 0.0f)
                {
                    throwVelocityY = -throwVelocityY;
                }

                int enemyIdx;
                int monsterType;

                if (selectedLevel == 1)
                {
                    enemyIdx = backpackLevel1[--backCountLevel1];
                    monsterType = enemyTypes[enemyIdx];
                }
                else if (selectedLevel == 2)
                {
                    enemyIdx = backpackLevel2[--backCountLevel2];
                    monsterType = enemyTypes[enemyIdx];
                }

                enemyDisappeared[enemyIdx] = false;
                enemyThrown[enemyIdx] = true;
                enemyThrowVelocityY[enemyIdx] = throwVelocityY;
                enemyThrowVelocityX[enemyIdx] = 0;
                enemyX[enemyIdx] = player_x;
                enemyY[enemyIdx] = player_y;
                enemySucked[enemyIdx] = false;
            }

            const int enemySize = 64;
            for (int ei = 0; ei < MAX_ENEMIES; ei++)
            {
                if (!enemyThrown[ei])
                    continue;

                if (enemyThrowVelocityX[ei] != 0)
                {
                    float nextX = enemyX[ei] + enemyThrowVelocityX[ei];
                    int enemyRowTop = (int)(enemyY[ei] / cell_size);
                    int enemyRowMid = (int)((enemyY[ei] + enemySize / 2) / cell_size);
                    int enemyRowBottom = (int)((enemyY[ei] + enemySize - 1) / cell_size);
                    int enemyCol;

                    if (enemyThrowVelocityX[ei] > 0)
                    {
                        enemyCol = (int)((nextX + enemySize) / cell_size);
                    }
                    else
                    {
                        enemyCol = (int)(nextX / cell_size);
                    }

                    char tileTop = getTile(lvl, enemyRowTop, enemyCol, height, width);
                    char tileMid = getTile(lvl, enemyRowMid, enemyCol, height, width);
                    char tileBottom = getTile(lvl, enemyRowBottom, enemyCol, height, width);

                    if (tileTop == '#' || tileTop == '/' || tileTop == '\\' ||
                        tileMid == '#' || tileMid == '/' || tileMid == '\\' ||
                        tileBottom == '#' || tileBottom == '/' || tileBottom == '\\')
                    {
                        enemyDisappeared[ei] = true;
                        enemyThrown[ei] = false;
                    }
                    else
                    {
                        enemyX[ei] = nextX;
                    }
                }
                else if (enemyThrowVelocityY[ei] != 0)
                {
                    float nextY = enemyY[ei] + enemyThrowVelocityY[ei];
                    int enemyColLeft = (int)(enemyX[ei] / cell_size);
                    int enemyColMid = (int)((enemyX[ei] + enemySize / 2) / cell_size);
                    int enemyColRight = (int)((enemyX[ei] + enemySize - 1) / cell_size);
                    int enemyRow;

                    if (enemyThrowVelocityY[ei] > 0)
                    {
                        enemyRow = (int)((nextY + enemySize) / cell_size);
                    }
                    else
                    {
                        enemyRow = (int)(nextY / cell_size);
                    }

                    char tileLeft = getTile(lvl, enemyRow, enemyColLeft, height, width);
                    char tileMid = getTile(lvl, enemyRow, enemyColMid, height, width);
                    char tileRight = getTile(lvl, enemyRow, enemyColRight, height, width);

                    if (tileLeft == '#' || tileLeft == '/' || tileLeft == '\\' ||
                        tileMid == '#' || tileMid == '/' || tileMid == '\\' ||
                        tileRight == '#' || tileRight == '/' || tileRight == '\\')
                    {
                        enemyDisappeared[ei] = true;
                        enemyThrown[ei] = false;
                    }
                    else
                    {
                        enemyY[ei] = nextY;
                    }
                }
            }

            // Reduce cooldown
            if (damageCooldown > 0.0f)
            {
                damageCooldown -= 1.0f / 60.0f;
            }

            // Reduce life and increase cool down if monster in proximity
            bool anyThrown = false;
            for (int ti = 0; ti < MAX_ENEMIES; ti++)
                if (enemyThrown[ti])
                {
                    anyThrown = true;
                    break;
                }

            // If any per-enemy fireball actually hit the player, treat as damage
            bool anyFireballHit = false;
            for (int fi = 0; fi < MAX_ENEMIES; fi++)
                if (fireballHitArr[fi])
                {
                    anyFireballHit = true;
                    break;
                }

            if (!vacuum && (activeMonsterCollision || anyFireballHit) && (!anyThrown) &&
                damageCooldown <= 0.0f)
            {
                // Reduce life till -1 (to check for last life)
                if (lifeCount > -1)
                {
                    lifeCount -= 1;
                }
                damageCooldown = 2.0f;
            }

            // Clear fireball hit flags after applying damage so single hit counts once
            if (anyFireballHit)
            {
                for (int fi = 0; fi < MAX_ENEMIES; fi++)
                    fireballHitArr[fi] = false;
            }

            window.draw(playerLogoSpr);
            window.draw(playerNumSpr);

            // Display lives at a distance according to life count
            for (int i = 0; i < lifeCount; ++i)
            {
                window.draw(heartSpr[i]);
                heartSpr[i].setPosition(heartPosition + heartDistance, heartDistance);
                heartPosition += 64 + 10;
            }

            // Level progression from 1 to 2, and 2 to main menu
            bool allEnemiesGone = true;
            for (int i = 0; i < MAX_ENEMIES; i++)
            {
                if (!enemyDisappeared[i])
                {
                    allEnemiesGone = false;
                    break;
                }
            }
            if (!victoryAnimation && allEnemiesGone && ((backCountLevel1 == 0 && selectedLevel == 1) || (backCountLevel2 == 0 && selectedLevel == 2)))
            {
                victoryAnimation = true;
                victoryTimer = 0.0;
            }

            if (victoryAnimation)
            {
                victoryTimer += 1 / 60.0f;
                int animIndex = (int)(victoryTimer / 0.25f) % 4;
                victorySpr[animIndex].setScale(2, 2);
                victorySpr[animIndex].setPosition(player_x, player_y);
                window.draw(victorySpr[animIndex]);

                if (victoryTimer > 4.0f)
                {
                    victoryAnimation = false;

                    if (selectedLevel == 1)
                    {
                        backCountLevel1 = 0;
                        backCountLevel2 = 0;
                        selectedLevel = 2;
                        startLevel(selectedLevel, height, width, lifeCount, player_x, player_y, velocityY, onGround, damageCooldown, dropCooldown,
                                lvl, bgTex, bgSprite, blockTexture, blockSprite, oneWayTexture, oneWaySprite,
                                slopeTexture, slopeSprite, slopeBotTexture, slopeBotSprite, spacePressed, lvlMusic,
                                enemyTypes, enemyX, enemyY, enemyDisappeared, enemySucked, enemyThrown, 
                                enemyVelocityY, enemyGoingRight, enemyThrowVelocityX, enemyThrowVelocityY, 
                                MAX_ENEMIES, cell_size);
                    }
                    else if (selectedLevel == 2)
                    {
                        gameState = 0;
                        lvlMusic.stop();
                    }
                }
            }

            // Go to main menu and reset after negative life count
            if (lifeCount < 0)
            {
                for (int i = 0; i < 3; ++i)
                {
                    backpackLevel1[i] = 4; // 4 is not any monster's ID
                }

                for (int i = 0; i < 3; ++i)
                {
                    backpackLevel2[i] = 4;
                }

                backCountLevel1 = 0;
                backCountLevel2 = 0;
                gameState = 0;
                lvlMusic.stop();
            }
        }

        window.display();
    }

    lvlMusic.stop();
    for (int i = 0; i < height; i++)
        delete[] lvl[i];
    delete[] lvl;

    return 0;
}