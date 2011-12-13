#include "game.h"
#include "util/bitmap.h"
#include "util/debug.h"
#include "util/input/input-map.h"
#include "util/input/input-source.h"
#include "util/input/input-manager.h"
#include "util/input/keyboard.h"
#include "util/file-system.h"
#include "util/events.h"
#include <math.h>
#include <vector>
#include <algorithm>

using std::vector;

namespace Asteroids{

/* hacks */
const int GFX_X = 640;
const int GFX_Y = 480;

enum GameInput{
    Quit
};

class Star{
public:
    Star(int x, int y, double brightness):
    x(x),
    y(y),
    brightness(brightness){
    }

    void draw(const Graphics::Bitmap & work) const {
        int c = 255 * brightness;
        work.putPixel(x, y, Graphics::makeColor(c, c, c));
    }

    int x;
    int y;
    double brightness;
};

class StarField{
public:
    StarField(){
        for (int i = 0; i < 1000; i++){
            stars.push_back(makeStar());
        }
    }

    void draw(const Graphics::Bitmap & work) const {
        for (vector<Star>::const_iterator it = stars.begin(); it != stars.end(); it++){
            const Star & star = *it;
            star.draw(work);
        }
    }

    Star makeStar(){
        return Star(Util::rnd(GFX_X), Util::rnd(GFX_Y), Util::rnd(100) / 100.0);
    }

    vector<Star> stars;
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
        asteroidLarge = loadSprites(Filesystem::RelativePath("asteroids/large"));
        asteroidMedium = loadSprites(Filesystem::RelativePath("asteroids/medium"));
        asteroidSmall = loadSprites(Filesystem::RelativePath("asteroids/small"));

        explodeSmall = loadSprites(Filesystem::RelativePath("asteroids/small-explode"));
        explode = loadSprites(Filesystem::RelativePath("asteroids/explode"));

        ship1 = Util::ReferenceCount<Graphics::Bitmap>(new Graphics::Bitmap(Storage::instance().find(Filesystem::RelativePath("asteroids/ships/ship1.png")).path()));
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
                int use = tick / animationRate;
                if (use >= explode.size()){
                    return Util::ReferenceCount<Graphics::Bitmap>(NULL);
                }
                return explode[use];
                break;
            }
            case ExplosionSmall: {
                int use = tick / animationRate;
                if (use >= explodeSmall.size()){
                    return Util::ReferenceCount<Graphics::Bitmap>(NULL);
                }
                return explodeSmall[use];
                break;
            }
        }
    }

    Util::ReferenceCount<Graphics::Bitmap> getAsteroidSprite(AsteroidSize size, int tick) const {
        int animationRate = 5;
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
    ticker(0){
        switch (size){
            case Large: life = 10; break;
            case Medium: life = 5; break;
            case Small: life = 1; break;
        }
    }

    void draw(const SpriteManager & manager, const Graphics::Bitmap & work){
        manager.getAsteroidSprite(size, ticker)->drawCenter((int) x, (int) y, work);
    }

    double getX() const {
        return x;
    }

    double getY() const {
        return y;
    }

    /* took a shot, lose some life. return true if dead. */
    bool hit(){
        life -= 1;
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

    Player(int x, int y):
    turnSpeed(4),
    x(x), y(y),
    angle(0),
    velocityX(0), velocityY(0),
    speed(0.2),
    addShot(false),
    shotCounter(0){
        input.set(Keyboard::Key_UP, Thrust);
        input.set(Keyboard::Key_DOWN, ReverseThrust);
        input.set(Keyboard::Key_LEFT, TurnLeft);
        input.set(Keyboard::Key_RIGHT, TurnRight);
        input.set(Keyboard::Key_SPACE, Shoot);
    }

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

    double getX(){
        return x;
    }

    double getY(){
        return y;
    }

    void doInput(){
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
            shoot();
        } else {
            resetShoot();
        }
    }

    void shoot(){
        if (shotCounter == 0){
            addShot = true;
            shotCounter = 10;
        }
    }

    void resetShoot(){
        addShot = false;
        shotCounter = 0;
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
    }

    void decreaseSpeed(){
        thrust(-speed);
    }

    void turnLeft(){
        angle += turnSpeed;
    }

    void turnRight(){
        angle -= turnSpeed;
    }

    void logic(World & world){
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

        if (shotCounter > 0){
            shotCounter -= 1;
        }

        if (addShot){
            addShot = false;
            createBullet(world);
        }

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

    void draw(const SpriteManager & manager, const Graphics::Bitmap & work){
        Util::ReferenceCount<Graphics::Bitmap> sprite = manager.getPlayer();
        /* sprite is facing at 90 degrees so we rotate it by 90 to make it face 0 first */
        sprite->drawPivot(sprite->getWidth() / 2, sprite->getHeight() / 2, (int) x, (int) y, angle - 90, work);
    }

    void createBullet(World & world);

protected:
    double x, y;
    int angle;
    double velocityX;
    double velocityY;
    double speed;

    bool addShot;
    int shotCounter;
};

class World{
public:
    World():
    player(GFX_X / 2, GFX_Y / 2){
        for (int i = 0; i < 10; i++){
            asteroids.push_back(makeAsteroid(Large));
        }
    }

    StarField stars;
    SpriteManager manager;
    vector<Util::ReferenceCount<Asteroid> > asteroids;
    Player player;
    vector<Util::ReferenceCount<Bullet> > bullets;
    vector<Util::ReferenceCount<Explosion> > explosions;

    bool nearPlayer(int x, int y){
        return Util::distance(player.getX(), player.getY(), x, y) < 100;
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
        while (nearPlayer(x, y)){
            x = Util::rnd(GFX_X);
            y = Util::rnd(GFX_Y);
        }
        return makeAsteroid(x, y, size);
    }

    Util::ReferenceCount<Asteroid> makeAsteroid(double x, double y, AsteroidSize size){
        return Util::ReferenceCount<Asteroid>(new Asteroid(x, y, Util::rnd(360), Util::rnd(10) / 7.0 + 0.5, size));
    }

    void logic(){
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

        player.logic(*this);

        enforceConstraints();
        doCollisions();
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

    void doCollisions(){
        for (vector<Util::ReferenceCount<Bullet> >::iterator it = bullets.begin(); it != bullets.end(); /**/ ){
            Util::ReferenceCount<Bullet> bullet = *it;
            Util::ReferenceCount<Asteroid> asteroid = findAsteroid(bullet->getX(), bullet->getY(), bullet->getRadius());
            if (asteroid != NULL){
                addExplosion(bullet->getX(), bullet->getY(), ExplosionSmall);
                if (asteroid->hit()){
                    addExplosion(asteroid->getX(), asteroid->getY(), ExplosionLarge);
                    removeAsteroid(asteroid);
                    asteroid->createMore(*this);
                }

                it = bullets.erase(it);
            } else {
                it++;
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

        for (vector<Util::ReferenceCount<Explosion> >::iterator it = explosions.begin(); it != explosions.end(); it++){
            Util::ReferenceCount<Explosion> explosion = *it;
            explosion->draw(manager, work);
        }

        player.draw(manager, work);
    }

    void addBullet(double x, double y, int angle, double speed){
        bullets.push_back(Util::ReferenceCount<Bullet>(new Bullet(x, y, angle, speed)));
    }
};

void Asteroid::createMore(World & world){
    switch (size){
        case Large: {
            int many = Util::rnd(4) + 1;
            for (int i = 0; i < many; i++){
                world.addAsteroid(world.makeAsteroid(x, y, Medium));
            }
            break;
        }
        case Medium: {
            int many = Util::rnd(4) + 1;
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

void Player::createBullet(World & world){
    world.addBullet(x, y, angle, Util::distance(0, 0, velocityX, velocityY) + 3);
}

class Game: public Util::Logic, public Util::Draw {
public:
    Game():
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
        return system;
    }

    bool done(){
        return quit;
    }

    void draw(const Graphics::Bitmap & work){
        work.clear();
        world.draw(work);
        work.BlitToScreen();
    }

protected:
    bool quit;
};

void run(){
    Global::debug(0) << "Asteroids!" << std::endl;

    Keyboard::pushRepeatState(false);
    Game game;
    Util::standardLoop(game, game);
    Keyboard::popRepeatState();
}

}
