#include "game.h"
#include "r-tech1/graphics/bitmap.h"
#include "r-tech1/debug.h"
#include "r-tech1/font.h"
#include "r-tech1/input/input-map.h"
#include "r-tech1/input/input-source.h"
#include "r-tech1/input/input-manager.h"
#include "r-tech1/input/keyboard.h"
#include "r-tech1/file-system.h"
#include "r-tech1/sound/music.h"
#include "r-tech1/sound/sound.h"
#include "r-tech1/events.h"
#include "r-tech1/init.h"
#include "r-tech1/configuration.h"
#include <math.h>
#include <vector>
#include <algorithm>

using std::vector;

namespace Asteroids{

/* hacks */
const int GFX_X = 1024;
const int GFX_Y = 768;

enum GameInput{
    Quit
};

class Star{
public:
    Star(int x, int y, double brightness):
    x(x),
    y(y),
    brightness(brightness),
    brightModifier(0),
    velocity(0){
        //Global::debug(0) << "Brightness: " << 255 * brightness << std::endl;
        const int c = 255 * brightness;
        if (c < 60){
            velocity = .2;
        } else if (c >= 60 && c < 180){
            velocity = .8;
        } else {
            velocity = 1.5;
        }
    }
    
    void move(int angle, double velocityX, double velocityY){
        x -= cos(Util::radians(angle)) * (velocity * velocityX);
        y += sin(Util::radians(angle)) * (velocity * velocityY);
        if (x < 0){
            x = GFX_X + x;
        } else if (x > GFX_X){
            x = x - GFX_X;
        }
        if (y < 0){
            y = GFX_Y + y;
        } else if (y > GFX_Y){
            y = y - GFX_Y;
        }
    }
    
    void moveVertical(bool reverse){
        if (reverse){
            const double v = y + -velocity;
            y = (v < 0 ? GFX_Y + v : v);
        } else {
            y = fmod(y + velocity, GFX_Y);
        }
    }
    
    void moveHorizontal(bool reverse){
        if (reverse){
            const double v = x + -velocity;
            x = (v < 0 ? GFX_X + v : v);
        } else {
            x = fmod(x + velocity, GFX_X);
        }
    }
    
    void logic() {
        brightModifier = Util::rnd(20) / 100.0;
    }

    void draw(const Graphics::Bitmap & work) const {
        int c = 255 * (brightness + brightModifier);
        work.putPixel(x, y, Graphics::makeColor(c, c, c));
    }

    double x;
    double y;
    double brightness;
    double brightModifier;
    double velocity;
};

class StarField{
public:
    StarField():
    background(GFX_X, GFX_Y),
    direction(Util::rnd(50) < 25){
        background.clear();
        for (int i = 0; i < 1000; i++){
            stars.push_back(makeStar());
        }
    }
    
    void logic(int angle, double velocityX, double velocityY) {
        for (std::vector<Util::ReferenceCount<Star> >::const_iterator i = stars.begin(); i != stars.end(); ++i){
            Util::ReferenceCount<Star> star = *i;
            star->logic();
            star->move(angle, velocityX, velocityY);
        }
    }

    void draw(const Graphics::Bitmap & work) const {
        background.clear();
        for (std::vector<Util::ReferenceCount<Star> >::const_iterator i = stars.begin(); i != stars.end(); ++i){
            Util::ReferenceCount<Star> star = *i;
            star->draw(background);
        }
        background.draw(0, 0, work);
    }

    Util::ReferenceCount<Star> makeStar(){
        return Util::ReferenceCount<Star>(new Star(Util::rnd(GFX_X), Util::rnd(GFX_Y), Util::rnd(80) / 100.0));
    }

    vector<Util::ReferenceCount<Star> > stars;
    Graphics::Bitmap background;
    bool direction;
};

enum ExplodeSize{
    ExplosionLarge,
    ExplosionSmall
};

enum AsteroidSize{
    Small,
    Medium,
    Large
};

class SpriteManager{
public:
    SpriteManager(){
        asteroidLarge = loadSprites(Filesystem::RelativePath("large"));
        asteroidMedium = loadSprites(Filesystem::RelativePath("medium"));
        asteroidSmall = loadSprites(Filesystem::RelativePath("small"));

        explodeSmall = loadSprites(Filesystem::RelativePath("small-explode"));
        explode = loadSprites(Filesystem::RelativePath("explode"));

        ship1 = Util::ReferenceCount<Graphics::Bitmap>(new Graphics::Bitmap(Storage::instance().find(Filesystem::RelativePath("ships/ship1.png")).path()));
    }

    vector<Util::ReferenceCount<Graphics::Bitmap> > loadSprites(const Filesystem::RelativePath & path){
        vector<Util::ReferenceCount<Graphics::Bitmap> > sprites;
        vector<Filesystem::AbsolutePath> largeSprites = Storage::instance().getFiles(Storage::instance().find(path), "*.png");
        std::sort(largeSprites.begin(), largeSprites.end());
        for (vector<Filesystem::AbsolutePath>::iterator it = largeSprites.begin(); it != largeSprites.end(); it++){
            const Filesystem::AbsolutePath & path = *it;
            sprites.push_back(Util::ReferenceCount<Graphics::Bitmap>(new Graphics::Bitmap(path.path())));
        }
        return sprites;
    }
    
    Util::ReferenceCount<Graphics::Bitmap> getPlayer() const {
        return ship1;
    }

    Util::ReferenceCount<Graphics::Bitmap> getExplode(ExplodeSize size, int tick) const {
        int animationRate = 3;
        switch (size){
            case ExplosionLarge: {
                unsigned int use = tick / animationRate;
                if (use >= explode.size()){
                    return Util::ReferenceCount<Graphics::Bitmap>(NULL);
                }
                return explode[use];
                break;
            }
            case ExplosionSmall: {
                unsigned int use = tick / animationRate;
                if (use >= explodeSmall.size()){
                    return Util::ReferenceCount<Graphics::Bitmap>(NULL);
                }
                return explodeSmall[use];
                break;
            }
        }
        return Util::ReferenceCount<Graphics::Bitmap>(NULL);
    }

    Util::ReferenceCount<Graphics::Bitmap> getAsteroidSprite(AsteroidSize size, int tick) const {
        int animationRate = 4;
        switch (size){
            case Large: {
                return asteroidLarge[(tick / animationRate) % asteroidLarge.size()];
            }
            case Medium: {
                return asteroidMedium[(tick / animationRate) % asteroidMedium.size()];
            }
            case Small: {
                return asteroidSmall[(tick / animationRate) % asteroidSmall.size()];
            }
        }
        return asteroidMedium[(tick / animationRate) % asteroidMedium.size()];
    }

protected:
    vector<Util::ReferenceCount<Graphics::Bitmap> > asteroidLarge;
    vector<Util::ReferenceCount<Graphics::Bitmap> > asteroidMedium;
    vector<Util::ReferenceCount<Graphics::Bitmap> > asteroidSmall;

    vector<Util::ReferenceCount<Graphics::Bitmap> > explodeSmall;
    vector<Util::ReferenceCount<Graphics::Bitmap> > explode;

    Util::ReferenceCount<Graphics::Bitmap> ship1;
};

class World;

class Asteroid{
public:
    Asteroid(int x, int y, int angle, double speed, AsteroidSize size):
    x(x), y(y),
    angle(angle),
    speed(speed),
    size(size),
    ticker(Util::rnd(100)){
        switch (size){
            case Large: life = 10; break;
            case Medium: life = 5; break;
            case Small: life = 1; break;
        }
    }

    void draw(const SpriteManager & manager, const Graphics::Bitmap & work){
        manager.getAsteroidSprite(size, ticker)->drawCenter((int) x, (int) y, work);
    }

    AsteroidSize getSize() const {
        return size;
    }

    double getX() const {
        return x;
    }

    double getY() const {
        return y;
    }

    /* took a shot, lose some life. return true if dead. */
    bool hit(int amount){
        life -= amount;
        return life <= 0;
    }

    void createMore(World & world);

    int getRadius(const SpriteManager & manager){
        return manager.getAsteroidSprite(size, ticker)->getWidth() / 2;
    }
    
    bool touches(double x, double y, int radius, const SpriteManager & manager){
        return Util::distance(this->x, this->y, x, y) < radius + getRadius(manager);
    }

    void logic(){
        ticker += 1;
        x += cos(Util::radians(angle)) * speed;
        y -= sin(Util::radians(angle)) * speed;
        if (x < 0){
            x = GFX_X;
        }
        if (x > GFX_X){
            x = 0;
        }
        if (y < 0){
            y = GFX_Y;
        }
        if (y > GFX_Y){
            y = 0;
        }
    }

protected:
    double x, y;
    int angle;
    double speed;
    AsteroidSize size;
    int ticker;
    int life;
};

class Explosion{
public:
    Explosion(double x, double y, ExplodeSize size):
    x(x), y(y), size(size), ticker(0){
    }

    void logic(){
        ticker += 1;
    }

    bool isDead(const SpriteManager & manager){
        return manager.getExplode(size, ticker) == NULL;
    }

    void draw(const SpriteManager & manager, const Graphics::Bitmap & work){
        Util::ReferenceCount<Graphics::Bitmap> sprite = manager.getExplode(size, ticker);
        if (sprite != NULL){
            sprite->drawCenter((int) x, (int) y, work);
        }
    }

    double x, y;
    ExplodeSize size;
    int ticker;
};

class Bullet{
public:
    Bullet(double x, double y, int angle, double speed):
    x(x), y(y),
    angle(angle),
    speed(speed),
    radius(3){
    }

    void logic(){
        move();
    }

    void move(){
        x += cos(Util::radians(angle)) * speed;
        y -= sin(Util::radians(angle)) * speed;
    }

    int getRadius(){
        return radius;
    }

    double getX(){
        return x;
    }

    double getY(){
        return y;
    }

    void draw(const SpriteManager & manager, const Graphics::Bitmap & work){
        work.circleFill((int) x, (int) y, radius, Graphics::makeColor(0, 255, 0));
    }

protected:
    double x, y;
    int angle;
    double speed;
    int radius;
};

class ShootingBehavior;

static const double gravity = 0.02;
class Player{
public:
    enum Keys{
        Thrust,
        ReverseThrust,
        TurnLeft,
        TurnRight,
        Shoot
    };

    Player(int x, int y);

    struct Hold{
        Hold():
        thrust(false),
        reverseThrust(false),
        left(false),
        right(false),
        shoot(false){
        }

        bool thrust;
        bool reverseThrust;
        bool left;
        bool right;
        bool shoot;
    };

    Hold hold;
    InputMap<Keys> input;
    InputSource source;
    const int turnSpeed;
    bool alive;
    int score;

    double getX(){
        return x;
    }

    double getY(){
        return y;
    }

    int getScore() const {
        return score;
    }

    int getAngle() const {
        return angle;
    }

    double getVelocityX() const {
        return velocityX;
    }

    double getVelocityY() const {
        return velocityY;
    }

    void increaseScore(int amount){
        score += amount;
    }

    void loseScore(int amount){
        score -= amount;
    }

    void setShootingBehavior(const Util::ReferenceCount<ShootingBehavior> & behavior){
        this->shootBehavior = behavior;
    }

    void spawn(double x, double y);

    void doInput();

    int getRadius(const SpriteManager & manager){
        Util::ReferenceCount<Graphics::Bitmap> sprite = manager.getPlayer();
        /* average the width and height, then divide by 2 to get the radius */
        return (sprite->getWidth() + sprite->getHeight()) / 4;
    }

    void thrust(double amount){
        velocityX += cos(Util::radians(angle)) * amount;
        velocityY += -sin(Util::radians(angle)) * amount;
        if (velocityX > 2){
            velocityX = 2;
        }
        if (velocityX < -2){
            velocityX = -2;
        }
        if (velocityY > 2){
            velocityY = 2;
        }
        if (velocityY < -2){
            velocityY = -2;
        }

    }

    void increaseSpeed(){
        thrust(speed);
        if (accelerate < 10){
            accelerate += 2;
        }
    }

    void decreaseSpeed(){
        thrust(-speed);
        accelerate = 0;
    }

    void turnLeft(){
        angle += turnSpeed;
    }

    void turnRight(){
        angle -= turnSpeed;
    }

    bool isAlive(){
        return alive;
    }

    void setAlive(bool what){
        alive = what;
    }

    void logic(World & world);

    void draw(const SpriteManager & manager, const Graphics::Bitmap & work){
        Util::ReferenceCount<Graphics::Bitmap> sprite = manager.getPlayer();

        if (accelerate > 0){

            /* Draw two traingles that represent thrust. The first is yellow, meaning somewhat low energy. The second
             * is drawn on top of the first triangle and is red, which represents high energy.
             */

            /* This could probably be simplified using some matrix rotation/translations */
            int x1 = (int) (x + cos(Util::radians(angle + 180)) * (30 + accelerate));
            int y1 = (int) (y + -sin(Util::radians(angle + 180)) * (30 + accelerate));
            int x2 = (int) (x + cos(Util::radians(angle + 180)) * 5 + cos(Util::radians(angle - 90)) * 9);
            int y2 = (int) (y + -sin(Util::radians(angle + 180)) * 5 + -sin(Util::radians(angle - 90)) * 9);
            int x3 = (int) (x + cos(Util::radians(angle + 180)) * 5 + cos(Util::radians(angle + 90)) * 9);
            int y3 = (int) (y + -sin(Util::radians(angle + 180)) * 5 + -sin(Util::radians(angle + 90)) * 9);

            work.triangle(x1, y1, x2, y2, x3, y3, Graphics::makeColor(255, 255, 0));

            x1 = (int) (x + cos(Util::radians(angle + 180)) * (30 + accelerate / 2));
            y1 = (int) (y + -sin(Util::radians(angle + 180)) * (30 + accelerate / 2));
            x2 = (int) (x + cos(Util::radians(angle + 180)) * 5 + cos(Util::radians(angle - 90)) * 7);
            y2 = (int) (y + -sin(Util::radians(angle + 180)) * 5 + -sin(Util::radians(angle - 90)) * 7);
            x3 = (int) (x + cos(Util::radians(angle + 180)) * 5 + cos(Util::radians(angle + 90)) * 7);
            y3 = (int) (y + -sin(Util::radians(angle + 180)) * 5 + -sin(Util::radians(angle + 90)) * 7);

            work.triangle(x1, y1, x2, y2, x3, y3, Graphics::makeColor(255, 128, 0));
        }

        /* sprite is facing at 90 degrees so we rotate it by 90 to make it face 0 first */
        sprite->drawPivot(sprite->getWidth() / 2, sprite->getHeight() / 2, (int) x, (int) y, angle - 90, work);
    }

protected:
    double x, y;
    int angle;
    double velocityX;
    double velocityY;
    double speed;

    int accelerate;

    Util::ReferenceCount<ShootingBehavior> shootBehavior;
};

class ShootingBehavior{
public:
    ShootingBehavior():
        shootSound(Storage::instance().find(Filesystem::RelativePath("sounds/laser.wav")).path()),
        addShot(false),
        shotCounter(0){
    }

private:
    Sound shootSound;
    bool addShot;
    int shotCounter;

public:
    virtual void shoot(){
        if (shotCounter == 0){
            shootSound.play();
            addShot = true;
            shotCounter = 10;
        }
    }

    virtual void resetShoot(){
        addShot = false;
        shotCounter = 0;
    }

    virtual void createBullet(Player & player, World & world);

    virtual void logic(Player & player, World & world){
        if (shotCounter > 0){
            shotCounter -= 1;
        }

        if (addShot){
            addShot = false;
            createBullet(player, world);
        }
    }
};

class TripleShot: public ShootingBehavior {
public:
    virtual void createBullet(Player & player, World & world);
};

Player::Player(int x, int y):
    source(true),
    turnSpeed(4),
    alive(true),
    score(0),
    x(x), y(y),
    angle(0),
    velocityX(0), velocityY(0),
    speed(0.2),
    accelerate(0),
    shootBehavior(new ShootingBehavior()){
        input.set(Keyboard::Key_UP, Thrust);
        input.set(Keyboard::Key_DOWN, ReverseThrust);
        input.set(Keyboard::Key_LEFT, TurnLeft);
        input.set(Keyboard::Key_RIGHT, TurnRight);
        input.set(Keyboard::Key_SPACE, Shoot);

        input.set(Joystick::Up, Thrust);
        input.set(Joystick::Down, ReverseThrust);
        input.set(Joystick::Left, TurnLeft);
        input.set(Joystick::Right, TurnRight);
        input.set(Joystick::Button1, Shoot);
        input.set(Joystick::Button2, Shoot);
        input.set(Joystick::Button3, Shoot);
        input.set(Joystick::Button4, Shoot);
}

void Player::spawn(double x, double y){
    this->x = x;
    this->y = y;
    hold = Hold();
    shootBehavior = new ShootingBehavior();
    velocityX = 0;
    velocityY = 0;
}

void Player::logic(World & world){
    doInput();

    if (velocityX > gravity){
        velocityX -= gravity;
    } else if (velocityX < -gravity){
        velocityX += gravity;
    } else {
        velocityX = 0;
    }

    if (velocityY > gravity){
        velocityY -= gravity;
    } else if (velocityY < -gravity){
        velocityY += gravity;
    } else {
        velocityY = 0;
    }

    x += velocityX;
    y += velocityY;

    shootBehavior->logic(*this, world);

    if (x < 0){
        x = GFX_X;
    }
    if (x > GFX_X){
        x = 0;
    }
    if (y < 0){
        y = GFX_Y;
    }
    if (y > GFX_Y){
        y = 0;
    }

    if (accelerate > 0){
        accelerate -= 1;
    }
}

void Player::doInput(){
    class Handler: public InputHandler<Keys> {
    public:
        Handler(Hold & hold):
            hold(hold){
            }

        Hold & hold;

        void release(const Keys & input, Keyboard::unicode_t unicode){
            switch (input){
                case Thrust: {
                    hold.thrust = false;
                    break;
                }
                case ReverseThrust: {
                    hold.reverseThrust = false;
                    break;
                }
                case TurnLeft: {
                    hold.left = false;
                    break;
                }
                case TurnRight: {
                    hold.right = false;
                    break;
                }
                case Shoot: {
                    hold.shoot = false;
                    break;
                }
            }
        }

        void press(const Keys & input, Keyboard::unicode_t unicode){
            switch (input){
                case Thrust: {
                    hold.thrust = true;
                    break;
                }
                case ReverseThrust: {
                    hold.reverseThrust = true;
                    break;
                }
                case TurnLeft: {
                    hold.left = true;
                    break;
                }
                case TurnRight: {
                    hold.right = true;
                    break;
                }
                case Shoot: {
                    hold.shoot = true;
                    break;
                }
            }
        }
    };

    Handler handler(hold);
    InputManager::handleEvents(input, source, handler);

    if (hold.thrust){
        increaseSpeed();
    }

    if (hold.reverseThrust){
        decreaseSpeed();
    }

    if (hold.left){
        turnLeft();
    }

    if (hold.right){
        turnRight();
    }

    if (hold.shoot){
        shootBehavior->shoot();
    } else {
        shootBehavior->resetShoot();
    }
}

class Powerup{
public:
    Powerup(double x, double y, int angle, double speed):
    x(x),
    y(y),
    angle(angle),
    speed(speed),
    die(false),
    life(350),
    ball(0),
    ballAngle(0){
    }

    double x;
    double y;
    int angle;
    double speed;
    bool die;
    int life;

    int ball;
    int ballAngle;

    bool isDead(){
        return die;
    }

    void logic(World & world);

    void power(Player & player){
        player.setShootingBehavior(Util::ReferenceCount<ShootingBehavior>(new TripleShot()));
    }

    void draw(const Graphics::Bitmap & work){
        ball += 4;
        if (ball > 360){
            ball = ball - 360;
        }

        ballAngle += 2;
        if (ballAngle > 360){
            ballAngle = ballAngle - 360;
        }

        double distance = sin(Util::radians(ball)) * 7;
        int size = 4;

        int x1 = (int) (x + cos(Util::radians(ballAngle)) * distance);
        int y1 = (int) (y + -sin(Util::radians(ballAngle)) * distance);

        work.circleFill(x1, y1, size, Graphics::makeColor(0, 255, 0));

        x1 = (int) (x + cos(Util::radians(ballAngle + 120)) * distance);
        y1 = (int) (y + -sin(Util::radians(ballAngle + 120)) * distance);

        work.circleFill(x1, y1, size, Graphics::makeColor(255, 0, 0));

        x1 = (int) (x + cos(Util::radians(ballAngle - 120)) * distance);
        y1 = (int) (y + -sin(Util::radians(ballAngle - 120)) * distance);

        work.circleFill(x1, y1, size, Graphics::makeColor(64, 64, 255));
    }
};

class World{
public:
    World():
    player(GFX_X / 2, GFX_Y / 2),
    asteroidExplode(Storage::instance().find(Filesystem::RelativePath("sounds/explode.wav")).path()),
    bulletHit(Storage::instance().find(Filesystem::RelativePath("sounds/pop.wav")).path()),
    playerDie(Storage::instance().find(Filesystem::RelativePath("sounds/crash.wav")).path()),
    spawnPlayer(-1){
        for (int i = 0; i < Util::rnd(7) + 5; i++){
            asteroids.push_back(makeAsteroid(Large));
            // asteroids.push_back(makeAsteroid(Small));
        }

        // makePowerup(100, 100);
    }

    StarField stars;
    SpriteManager manager;
    vector<Util::ReferenceCount<Asteroid> > asteroids;
    Player player;
    vector<Util::ReferenceCount<Bullet> > bullets;
    vector<Util::ReferenceCount<Explosion> > explosions;
    vector<Util::ReferenceCount<Powerup> > powerups;

    Sound asteroidExplode;
    Sound bulletHit;
    Sound playerDie;
    
    /* -1 means do nothing
     * 0 means try to spawn the player
     * >0 means count down to 0
     */
    int spawnPlayer;

    bool nearPlayer(int x, int y){
        return Util::distance(player.getX(), player.getY(), x, y) < 100;
    }

    const Player & getPlayer() const {
        return player;
    }

    Player & getPlayer(){
        return player;
    }

    int closestAsteroid(int x, int y){
        int closest = -1;
        for (vector<Util::ReferenceCount<Asteroid> >::iterator it = asteroids.begin(); it != asteroids.end(); it++){
            Util::ReferenceCount<Asteroid> asteroid = *it;
            int distance = Util::distance(asteroid->getX(), asteroid->getY(), x, y);
            if (closest == -1 || distance < closest){
                closest = distance;
            }
        }
        return closest;
    }

    void addAsteroid(const Util::ReferenceCount<Asteroid> & asteroid){
        asteroids.push_back(asteroid);
    }

    void addExplosion(double x, double y, ExplodeSize size){
        explosions.push_back(Util::ReferenceCount<Explosion>(new Explosion(x, y, size)));
    }

    Util::ReferenceCount<Asteroid> makeAsteroid(AsteroidSize size){
        int x = Util::rnd(GFX_X);
        int y = Util::rnd(GFX_Y);
        while (player.isAlive() && nearPlayer(x, y)){
            x = Util::rnd(GFX_X);
            y = Util::rnd(GFX_Y);
        }
        return makeAsteroid(x, y, size);
    }

    void makePowerup(int x, int y){
        powerups.push_back(Util::ReferenceCount<Powerup>(new Powerup(x, y, Util::rnd(360), ((double) Util::rnd(3) + 1) / 3.0)));
    }

    Util::ReferenceCount<Asteroid> makeAsteroid(double x, double y, AsteroidSize size){
        return Util::ReferenceCount<Asteroid>(new Asteroid(x, y, Util::rnd(360), Util::rnd(10) / 7.0 + 0.5, size));
    }

    void logic(){
        stars.logic(player.getAngle(), player.getVelocityX(), player.getVelocityY());
        for (vector<Util::ReferenceCount<Asteroid> >::iterator it = asteroids.begin(); it != asteroids.end(); it++){
            Util::ReferenceCount<Asteroid> asteroid = *it;
            asteroid->logic();
        }
        for (vector<Util::ReferenceCount<Bullet> >::iterator it = bullets.begin(); it != bullets.end(); it++){
            Util::ReferenceCount<Bullet> bullet = *it;
            bullet->logic();
        }

        for (vector<Util::ReferenceCount<Explosion> >::iterator it = explosions.begin(); it != explosions.end(); it++){
            Util::ReferenceCount<Explosion> explosion = *it;
            explosion->logic();
        }

        for (vector<Util::ReferenceCount<Powerup> >::iterator it = powerups.begin(); it != powerups.end(); it++){
            Util::ReferenceCount<Powerup> powerup = *it;
            powerup->logic(*this);
        }

        if (player.isAlive()){
            player.logic(*this);
        } else {
            if (spawnPlayer > 0){
                spawnPlayer -= 1;
            } else if (spawnPlayer == 0){
                int x = Util::rnd(GFX_X);
                int y = Util::rnd(GFX_Y);
                if (closestAsteroid(x, y) > 100){
                    player.spawn(x, y);
                    player.setAlive(true);
                    spawnPlayer = -1;
                }
            }
        }

        enforceConstraints();
        doCollisions();

        if (asteroids.size() == 0){
            for (int i = 0; i < Util::rnd(7) + 5; i++){
                asteroids.push_back(makeAsteroid(Large));
            }
        }
    }

    Util::ReferenceCount<Asteroid> findAsteroid(double x, double y, int radius){
        for (vector<Util::ReferenceCount<Asteroid> >::iterator it = asteroids.begin(); it != asteroids.end(); it++){
            Util::ReferenceCount<Asteroid> asteroid = *it;
            if (asteroid->touches(x, y, radius, manager)){
                return asteroid;
            }
        }

        return Util::ReferenceCount<Asteroid>(NULL);
    }

    void removeAsteroid(const Util::ReferenceCount<Asteroid> & asteroid){
        for (vector<Util::ReferenceCount<Asteroid> >::iterator it = asteroids.begin(); it != asteroids.end(); /**/){
            if (*it == asteroid){
                it = asteroids.erase(it);
            } else {
                it++;
            }
        }
    }

    void hitAsteroid(const Util::ReferenceCount<Asteroid> & asteroid, int amount){
        if (asteroid->hit(amount)){
            player.increaseScore(50);
            asteroidExplode.play();
            addExplosion(asteroid->getX(), asteroid->getY(), ExplosionLarge);
            removeAsteroid(asteroid);
            asteroid->createMore(*this);
            if (asteroid->getSize() == Small && Util::rnd(20) == 0){
                makePowerup(asteroid->getX(), asteroid->getY());
            }
        }
    }

    bool touchPlayer(double x, double y, int radius){
        return Util::distance(x, y, player.getX(), player.getY()) < radius + player.getRadius(manager);
    }

    void doCollisions(){
        for (vector<Util::ReferenceCount<Bullet> >::iterator it = bullets.begin(); it != bullets.end(); /**/ ){
            Util::ReferenceCount<Bullet> bullet = *it;
            Util::ReferenceCount<Asteroid> asteroid = findAsteroid(bullet->getX(), bullet->getY(), bullet->getRadius());
            if (asteroid != NULL){
                player.increaseScore(10);
                addExplosion(bullet->getX(), bullet->getY(), ExplosionSmall);
                bulletHit.play();
                hitAsteroid(asteroid, 1);
                it = bullets.erase(it);
            } else {
                it++;
            }
        }

        if (player.isAlive()){
            Util::ReferenceCount<Asteroid> asteroid = findAsteroid(player.getX(), player.getY(), player.getRadius(manager));
            if (asteroid != NULL){
                addExplosion(player.getX(), player.getY(), ExplosionLarge);
                player.loseScore(500);
                playerDie.play();
                player.setAlive(false);
                spawnPlayer = 60;
                hitAsteroid(asteroid, 5);
            }
        }
    }

    void enforceConstraints(){
        for (vector<Util::ReferenceCount<Bullet> >::iterator it = bullets.begin(); it != bullets.end(); /**/){
            Util::ReferenceCount<Bullet> bullet = *it;
            if (outside(bullet->getX(), bullet->getY())){
                it = bullets.erase(it);
            } else {
                it++;
            }
        }

        for (vector<Util::ReferenceCount<Powerup> >::iterator it = powerups.begin(); it != powerups.end(); /**/){
            Util::ReferenceCount<Powerup> powerup = *it;
            if (powerup->isDead()){
                it = powerups.erase(it);
            } else {
                it++;
            }
        }
        
        for (vector<Util::ReferenceCount<Explosion> >::iterator it = explosions.begin(); it != explosions.end(); /**/){
            Util::ReferenceCount<Explosion> explosion = *it;
            if (explosion->isDead(manager)){
                it = explosions.erase(it);
            } else {
                it++;
            }
        }
    }

    bool outside(double x, double y){
        return x < 0 || y < 0 ||
               x > GFX_X || y > GFX_Y;
    }

    void draw(const Graphics::Bitmap & work){
        stars.draw(work);
        for (vector<Util::ReferenceCount<Asteroid> >::iterator it = asteroids.begin(); it != asteroids.end(); it++){
            Util::ReferenceCount<Asteroid> asteroid = *it;
            asteroid->draw(manager, work);
        }
        for (vector<Util::ReferenceCount<Bullet> >::iterator it = bullets.begin(); it != bullets.end(); it++){
            Util::ReferenceCount<Bullet> bullet = *it;
            bullet->draw(manager, work);
        }

        for (vector<Util::ReferenceCount<Powerup> >::iterator it = powerups.begin(); it != powerups.end(); it++){
            Util::ReferenceCount<Powerup> powerup = *it;
            powerup->draw(work);
        }

        for (vector<Util::ReferenceCount<Explosion> >::iterator it = explosions.begin(); it != explosions.end(); it++){
            Util::ReferenceCount<Explosion> explosion = *it;
            explosion->draw(manager, work);
        }

        if (player.isAlive()){
            player.draw(manager, work);
        }

        const Font & font = Font::getDefaultFont(20, 20);
        font.printf(1, 1, Graphics::makeColor(255, 255, 255), work, "Score %d", 0, player.getScore());
    }

    void addBullet(double x, double y, int angle, double speed){
        bullets.push_back(Util::ReferenceCount<Bullet>(new Bullet(x, y, angle, speed)));
    }
};

void Powerup::logic(World & world){
    if (life > 0){
        life -= 1;
    } else {
        die = true;
    }

    x += cos(angle) * speed;
    y += -sin(angle) * speed;

    if (x < 0){
        x = GFX_X;
    }
    if (x > GFX_X){
        x = 0;
    }
    if (y < 0){
        y = GFX_Y;
    }
    if (y > GFX_Y){
        y = 0;
    }

    if (world.touchPlayer(x, y, 10)){
        die = true;
        power(world.getPlayer());
    }
}

void ShootingBehavior::createBullet(Player & player, World & world){
    world.addBullet(player.getX(), player.getY(), player.getAngle(), Util::distance(0, 0, player.getVelocityX(), player.getVelocityY()) + 3);
}

void TripleShot::createBullet(Player & player, World & world){
    world.addBullet(player.getX(), player.getY(), player.getAngle(), Util::distance(0, 0, player.getVelocityX(), player.getVelocityY()) + 3);
    world.addBullet(player.getX(), player.getY(), player.getAngle() + 10, Util::distance(0, 0, player.getVelocityX(), player.getVelocityY()) + 3);
    world.addBullet(player.getX(), player.getY(), player.getAngle() - 10, Util::distance(0, 0, player.getVelocityX(), player.getVelocityY()) + 3);
}

void Asteroid::createMore(World & world){
    switch (size){
        case Large: {
            int many = Util::rnd(3) + 1;
            for (int i = 0; i < many; i++){
                world.addAsteroid(world.makeAsteroid(x, y, Medium));
            }
            break;
        }
        case Medium: {
            int many = Util::rnd(5) + 1;
            for (int i = 0; i < many; i++){
                world.addAsteroid(world.makeAsteroid(x, y, Small));
            }
            break;
        }
        case Small: {
            break;
        }
    }
}

class Game: public Util::Logic, public Util::Draw {
public:
    Game():
        source(true),
        quit(false){
            input.set(Keyboard::Key_ESC, Quit);
        }

    World world;
    InputMap<GameInput> input;
    InputSource source;

    void run(){
        world.logic();
        handleInput();
    }

    void handleInput(){
        class Handler: public InputHandler<GameInput> {
        public:
            Handler(Game & game):
                game(game){
                }

            Game & game;

            void release(const GameInput & input, Keyboard::unicode_t unicode){
            }

            void press(const GameInput & input, Keyboard::unicode_t unicode){
                switch (input){
                    case Quit: {
                        game.quit = true;
                        break;
                    }
                }
            }
        };

        Handler handler(*this);
        InputManager::handleEvents(input, source, handler);
    }

    double ticks(double system){
        return system * Global::ticksPerSecond(40);
    }

    bool done(){
        return quit;
    }

    void draw(const Graphics::Bitmap & buffer){
        Graphics::StretchedBitmap work(GFX_X, GFX_Y, buffer);
        work.start();
        work.clear();
        world.draw(work);
        work.finish();
        // buffer.BlitToScreen();
    }

protected:
    bool quit;
};

void run(){
    Global::debug(0) << "Asteroids!" << std::endl;

    Music::changeSong();
    Keyboard::pushRepeatState(false);
    Game game;
    Util::standardLoop(game, game);
    Keyboard::popRepeatState();
}

}
