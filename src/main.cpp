#include "r-tech1/init.h"
#include "r-tech1/debug.h"

#include "r-tech1/graphics/bitmap.h"

//#include "factory/collector.h"
#include "r-tech1/network/network.h"
#include "r-tech1/token_exception.h"
#include "r-tech1/system.h"
#include "r-tech1/sound/music.h"
#include "r-tech1/menu/menu.h"
#include "r-tech1/menu/menu-exception.h"
#include "r-tech1/menu/optionfactory.h"
#include "r-tech1/input/input-manager.h"
#include "r-tech1/exceptions/shutdown_exception.h"
#include "r-tech1/exceptions/exception.h"
#include "r-tech1/exceptions/load_exception.h"
#include "r-tech1/timedifference.h"
#include "r-tech1/funcs.h"
#include "r-tech1/ftalleg.h"
#include "r-tech1/file-system.h"
#include "r-tech1/tokenreader.h"
#include "r-tech1/token.h"
#include "r-tech1/parameter.h"
#include "r-tech1/version.h"
#include "r-tech1/debug.h"
#include "r-tech1/configuration.h"
#include "r-tech1/init.h"
#include "r-tech1/main.h"
#include "r-tech1/argument.h"
#include "argument.h"
#include "game.h"

using namespace std;

static const int DEFAULT_DEBUG = 0;

template <class X>
static void appendVector(vector<X> & to, const vector<X> & from){
    to.insert(to.end(), from.begin(), from.end());
}

static int startMain(const vector<Util::ReferenceCount<ArgumentAction> > & actions, bool allow_quit){
    while (true){
        bool normal_quit = false;
        try{
            for (vector<Util::ReferenceCount<ArgumentAction> >::const_iterator it = actions.begin(); it != actions.end(); it++){
                Util::ReferenceCount<ArgumentAction> action = *it;
                action->act();
            }
            normal_quit = true;
        } catch (const Filesystem::Exception & ex){
            Global::debug(0) << "There was a problem loading the main menu. Error was:\n  " << ex.getTrace() << endl;
        } catch (const TokenException & ex){
            Global::debug(0) << "There was a problem with the token. Error was:\n  " << ex.getTrace() << endl;
            return -1;
        } catch (const LoadException & ex){
            Global::debug(0) << "There was a problem loading the main menu. Error was:\n  " << ex.getTrace() << endl;
            return -1;
        } catch (const Exception::Return & ex){
        } catch (const ShutdownException & shutdown){
            Global::debug(1) << "Forced a shutdown. Cya!" << endl;
        } catch (const ReloadMenuException & ex){
            Global::debug(1) << "Menu Reload Requested. Restarting...." << endl;
            continue;
        } catch (const ftalleg::Exception & ex){
            Global::debug(0) << "Freetype exception caught. Error was:\n" << ex.getReason() << endl;
        } catch (const Exception::Base & base){
            // Global::debug(0) << "Freetype exception caught. Error was:\n" << ex.getReason() << endl;
            Global::debug(0) << "Base exception: " << base.getTrace() << endl;
/* android doesn't have bad_alloc for some reason */
// #ifndef ANDROID
        } catch (const std::bad_alloc & fail){
            Global::debug(0) << "Failed to allocate memory. Usage is " << System::memoryUsage() << endl;
// #endif
        } catch (...){
            Global::debug(0) << "Uncaught exception!" << endl;
        }

        if (allow_quit && normal_quit){
            break;
        } else if (normal_quit && !allow_quit){
        } else if (!normal_quit){
            break;
        }
    }
    return 0;
}


class DefaultGame: public ArgumentAction {
public:
    void act(){
        Asteroids::run();
    }
};

int main(int argc, char ** argv){
    /* -1 means use whatever is in the configuration */
    Global::InitConditions conditions;

    bool music_on = true;
    bool allow_quit = true;
    //Collector janitor;

    Version::setVersion(0, 0, 1);

    System::startMemoryUsage();

    Global::setDebug(DEFAULT_DEBUG);
    Global::setDefaultDebugContext("asteroids");
    vector<const char *> all_args;

    vector<string> stringArgs;
    for (int q = 1; q < argc; q++){
        stringArgs.push_back(argv[q]);
    }
    
    vector<Util::ReferenceCount<Argument> > arguments;
    appendVector(arguments, Asteroids::arguments());
    
    vector<Util::ReferenceCount<ArgumentAction> > actions;

    /* Sort of a hack but if we are already at the end of the argument list (because some
     * argument already reached the end) then we don't increase the argument iterator
     */
    for (vector<string>::iterator it = stringArgs.begin(); it != stringArgs.end(); (it != stringArgs.end()) ? it++ : it){
        for (vector<Util::ReferenceCount<Argument> >::iterator arg = arguments.begin(); arg != arguments.end(); arg++){
            Util::ReferenceCount<Argument> argument = *arg;
            if (argument->isArg(*it)){
                it = argument->parse(it, stringArgs.end(), actions);

                /* Only parse one argument */
                break;
            }
        }
    }

    Global::debug(0) << "Debug level: " << Global::getDebug() << endl;
    
    Global::debug(0) << "Asteroid version " << Version::getVersionString() << endl;

    if (! Global::init(conditions)){
        Global::debug(0) << "Could not initialize system" << endl;
        return -1;
    }

    if (! Global::dataCheck()){
        return -1;
    }

    Util::Parameter<Graphics::Bitmap*> use(Graphics::screenParameter, Graphics::getScreenBuffer());
    
    InputManager input;
    //Music music(music_on);

    /* If there are no actions then start asteroids */
    if (actions.size() == 0){
        actions.push_back(Util::ReferenceCount<ArgumentAction>(new DefaultGame()));
    }

    Util::Parameter<Util::ReferenceCount<Graphics::ShaderManager> > defaultShaderManager(Graphics::shaderManager, Util::ReferenceCount<Graphics::ShaderManager>(new Graphics::ShaderManager()));

    Util::Parameter<Util::ReferenceCount<Path::RelativePath> > defaultFont(Font::defaultFont, Util::ReferenceCount<Path::RelativePath>(new Path::RelativePath("fonts/arial.ttf")));

    Util::Parameter<Util::ReferenceCount<Menu::FontInfo> > defaultMenuFont(Menu::menuFontParameter, Util::ReferenceCount<Menu::FontInfo>(new Menu::RelativeFontInfo(*defaultFont.current(), Configuration::getMenuFontWidth(), Configuration::getMenuFontHeight())));

    startMain(actions, allow_quit);

    //Configuration::saveConfiguration();

    Global::debug(0) << "Bye!" << endl;
    Global::closeLog();
    return 0;
}
