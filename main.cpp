#include <iostream>
#include "EventSys.hpp"

struct EventType1 : Event 
{ 
    EventType1(int x_, int y_) : x{x_}, y{y_} {}; 
    int x, y;
};
struct EventType2 : Event {};
struct EventType3 : Event {};
struct EventType4 : Event {};

using MyEventSystem = EventSystem<EventType1, EventType2, EventType3, EventType4>;

enum struct SubscriptionTypes
{
    EVENT_TYPE_1,
    EVENT_TYPE_2,
    EVENT_TYPE_4
};

using MySubscriptionManager = SubscriptionManager<SubscriptionTypes, MyEventSystem::Subscriber>;

void subToEvents(MySubscriptionManager&);

int main()
{
    MyEventSystem eventSys;
    MySubscriptionManager subManager{ eventSys.getSubscriber() };

    subToEvents(subManager);

    auto const& publisher { eventSys.getPublisher() };

    //Publish an event of type EventType1 and pass in some data that will
    //be recieved by any subscriptions to this event type.
    EventType1 e1{1, 1};
    publisher.pub(e1);

    EventType2 e2{};
    publisher.pub(e2);

    EventType4 e4{};
    publisher.pub(e4);

    //this does nothing since there never a subscription to the type EventType3
    EventType3 e3{};
    publisher.pub(e3);

    //now we are no longer subscribed to the subscription associated with EVENT_TYPE_2
    subManager.unsub(SubscriptionTypes::EVENT_TYPE_2);

    //publishing an event of type EventType2 does nothing now
    publisher.pub(e2);
}

void subToEvents(MySubscriptionManager& subManager)
{
    subManager.sub<EventType1>(SubscriptionTypes::EVENT_TYPE_1, [](Event const& e)
    {
        std::cout << "EventType1 has been published! ";

        //print the data inside this event type.
        //Event::unpack will make sure you did not accidentally sub to a type 
        //that differs from the type you are trying to downcast to (in debug mode)
        auto const& evnt { e.unpack<EventType1>() };
        std::cout << "x = " << evnt.x << ", y = " << evnt.y << '\n';
    });

    subManager.sub<EventType2>(SubscriptionTypes::EVENT_TYPE_2, [](Event const& e)
    {
        std::cout << "EventType2 has been published!\n";
    });

    subManager.sub<EventType4>(SubscriptionTypes::EVENT_TYPE_4, [](Event const& e)
    {
        std::cout << "EventType4 has been published!\n";
    });
}