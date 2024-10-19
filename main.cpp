#include <iostream>
#include <functional> //std::function
#include <unordered_map>
#include <typeindex>
#include <utility> //std::move
#include <cstdint> //uint32_t
#include <cassert> 

struct Event 
{
    template <typename EventType>
    auto& unpack(Event& e) 
    {
#ifdef NDEBUG
        return static_cast<EventType&>(e);
#else
        auto* downCastPtr { dynamic_cast<EventType*>(&e) };
        assert(downCastPtr && "trying to do an invalid downcast");
        return *downCastPtr;
#endif
    }

protected:
    virtual ~Event()=default;
};

template <typename T, typename... Types>
concept IsTypeInPack = (std::is_same_v<T, Types> || ...);

template <typename... EventTs>
class EventSystem
{
public:
    auto const& getPublisher() const {return mPublisher;}
    auto& getSubscriber() {return mSubscriber;}

    static_assert((std::is_base_of_v<Event, EventTs> && ...), 
        "All event types must inherit from Event");

    using OnEventCallback = std::function<void(Event&)>;
    using SubscriptionID = std::size_t;

    struct Subscriber
    {
        Subscriber(EventSystem<EventTs...>& thisEventSys) : mThisEventSys{thisEventSys} {}

        template <typename EventType>
        SubscriptionID sub(OnEventCallback callback)
        {
            static_assert
            (
                IsTypeInPack<EventType, EventTs...>, 
                "The template type paramater passed to"
                " EventSystem::Subscriber::sub was not a valid event type for this EventSystem."
            );
            
            auto& callbackVector { mThisEventSys.mSubscriptions[typeid(EventType)] };
            callbackVector.push_back(std::move(callback));
            return callbackVector.size() - 1;
        }

        template <typename EventType>
        void unsub(SubscriptionID subID)
        {
            static_assert
            (
                IsTypeInPack<EventType, EventTs...>, 
                "The template type paramater passed to"
                " EventSystem::Subscriber::unsub was not a valid event type for this EventSystem."
            );

            auto it { mThisEventSys.mSubscriptions.find(typeid(EventType)) };
            if(it != mThisEventSys.mSubscriptions.end())
            {
                auto& callbackVector { it->second };
                callbackVector.erase(callbackVector.begin() + subID);

                if(callbackVector.empty())
                {
                    mThisEventSys.mSubscriptions.erase(it);
                }
            }
        }

    private:
        EventSystem<EventTs...>& mThisEventSys;
    };

    struct Publisher
    {
        Publisher(EventSystem<EventTs...> const& thisEventSys) : mThisEventSys{thisEventSys} {}
        
        template <typename EventType>
        void pub(EventType& e) const
        {
            static_assert
            (
                IsTypeInPack<EventType, EventTs...>, 
                "The template type paramater passed to"
                " EventSystem::pub was not a valid event type for this EventSystem."
            );
        
            //find the list of subscribers associated with this event type (if any)
            auto const it { mThisEventSys.mSubscriptions.find(typeid(e)) };
        
            if(it != mThisEventSys.mSubscriptions.end())
            {
                for(auto const& fn : it->second)
                    fn(e);
            }
        }

    private:
        EventSystem<EventTs...> const& mThisEventSys;
    };

    friend struct Subscriber;
    friend struct Publisher;

private:
    //A map from event types -> a list of subscription callbacks.
    std::unordered_map< std::type_index, std::vector<OnEventCallback> > mSubscriptions;

    //use getSubscriber() and getPublisher() to get access to these, allowing the 
    //user of this event system to subscribe/unsubscribe or publish to events 
    Subscriber mSubscriber {*this};
    Publisher  mPublisher  {*this};
};

struct EventType1 : Event 
{ 
    EventType1(int x_, int y_) : x{x_}, y{y_} {}; 
    int x, y;
};
struct EventType2 : Event {};
struct EventType3 : Event {};
struct EventType4 : Event {};

using MyEventSystem = EventSystem<EventType1, EventType2, EventType3, EventType4>;

int main()
{
    MyEventSystem eventSys;

    auto& subscriber { eventSys.getSubscriber() };
    
    auto eventType1ID = subscriber.sub<EventType1>([](Event& e)
    {
        auto& evnt { e.unpack<EventType1>(e) };

        std::cout << "EventType1 has been published! ";

        //print the data inside this event type
        std::cout << "x = " << evnt.x << ", y = " << evnt.y << '\n';
    });

    subscriber.sub<EventType2>([](Event& e)
    {
        std::cout << "EventType2 has been published!\n";
    });

    subscriber.sub<EventType4>([](Event& e)
    {
        std::cout << "EventType4 has been published!\n";
    });

    auto const& publisher { eventSys.getPublisher() };

    EventType1 e1{1, 1};
    publisher.pub(e1);

    EventType2 e2{};
    publisher.pub(e2);

    EventType4 e4{};
    publisher.pub(e4);

    //this does nothing since there was no subscription to EventType3
    EventType3 e3{};
    publisher.pub(e3);

    //now unsubscribed from EventType1
    subscriber.unsub<EventType1>(eventType1ID);

    //nothing responds to e1 being published now that the subscription was removed
    publisher.pub(e1);
}