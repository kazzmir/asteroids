#include "argument.h"
#include "r-tech1/debug.h"
#include "game.h"

using std::vector;
using std::string;
using std::endl;

namespace Asteroids{

class AsteroidsArgument: public Argument::Parameter {
public:
    vector<string> keywords() const {
        vector<string> out;
        out.push_back("asteroids");
        return out;
    }

    string description() const {
        return " : Run asteroids";
    }

    class Run: public Argument::Action {
    public:
        virtual void act(){
            run();
        }
    };

    vector<string>::iterator parse(vector<string>::iterator current, vector<string>::iterator end, Argument::ActionRefs & actions){
        actions.push_back(::Util::ReferenceCount<Argument::Action>(new Run()));
        return current;
    }
};

Argument::ParameterRefs arguments(){
    Argument::ParameterRefs all;
    all.push_back(::Util::ReferenceCount<Argument::Parameter>(new AsteroidsArgument()));
    return all;
}

}
