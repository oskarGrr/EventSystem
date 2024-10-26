#pragma once
#include <functional> //std::function
#include <unordered_map>
#include <typeindex>
#include <utility> //std::move
#include <cstdint> //uint32_t
#include <cassert> 
#include <ranges>

struct Event 
{
    //Perform a downcast and do a runtime check in debug mode.
    template <typename EventType>
    auto const& unpack() const
    {
#ifdef NDEBUG
        return static_cast<EventType const&>(e);
#else
        EventType const* downCastPtr { dynamic_cast<EventType const*>(this) };
        assert(downCastPtr && "trying to do an invalid downcast");
        return *downCastPtr;
#endif
    }

protected:
    virtual ~Event()=default;
};

template <typename T, typename... Types>
concept IsTypeInPack = (std::is_same_v<T, Types> || ...);

using SubscriptionID  = std::size_t;
using OnEventCallback = std::function<void(Event const&)>;

//Using this SubscriptionManager is optional, you can use the EventSystem without it.
//EventSubscriber should be EventSystem< ... >::Subscriber
//Enum is an enum type that you associate with subscriptions
template <typename Enum, typename EventSubscriber>
requires std::is_enum_v<Enum>
class SubscriptionManager
{
public:
    SubscriptionManager(EventSubscriber& subscriber) : mSubscriber{subscriber} {};

    ~SubscriptionManager()
    {
        ubsubFromAll();
    }

    //Returns false if subscriptionTag is already associated with a subscription.
    //(otherwise returns true if the subscription was successfully put into the event system)
    //This could be the case if you accidentally call this function twice with the same enum or
    //if you accidentally map two different enums to the same integer value... dont do this.
    template <typename EventType>
    bool sub(Enum subscriptionTag, OnEventCallback callback)
    {
        //return false: this enum tag is already associated with a subscription.
        if(mSubscriptions.contains(subscriptionTag))
            return false;

        auto const ID { mSubscriber.sub<EventType>(std::move(callback)) };
        mSubscriptions.try_emplace(subscriptionTag, typeid(EventType), ID);

        return true;
    }

    //Returns false if the unsubscription was not successful. This could be because you accidentally 
    //used the wrong template type paramater (EventType does not match the type the subscription is subscribed to),
    //or because you are not subscribed to this subscription at all.
    template <typename EventType>
    bool unsub(Enum subscriptionTag)
    {
        bool wasCallbackRemoved {false};

        if(auto it{mSubscriptions.find(subscriptionTag)}; it != mSubscriptions.end())
        {
            wasCallbackRemoved = mSubscriber.unsub<EventType>(it->second.second);
            if(wasCallbackRemoved) { mSubscriptions.erase(it); }
        }

        return wasCallbackRemoved;
    }

    void ubsubFromAll()
    {
        for(auto const& sub : mSubscriptions | std::views::values)
            mSubscriber.unsub(sub.second, sub.first);

        mSubscriptions.clear();
    }

private:
    EventSubscriber& mSubscriber;

    //The Enum tags differentiate between multiple subscriptions to the same event type
    std::unordered_map<Enum, std::pair<std::type_index, SubscriptionID> > mSubscriptions;
};

template <typename... EventTs>
class EventSystem
{
public:
    auto const& getPublisher() const {return mPublisher;}
    auto& getSubscriber() {return mSubscriber;}

    static_assert((std::is_base_of_v<Event, EventTs> && ...), 
        "All event types must inherit from Event");  

    struct Subscriber
    {
        template <typename EventType>
        [[nodiscard]] SubscriptionID sub(OnEventCallback callback)
        {
            static_assert
            (
                IsTypeInPack<EventType, EventTs...>, 
                "The template type paramater passed to"
                " EventSystem::Subscriber::sub was not a valid event type for this EventSystem."
            );
            
            auto& callbackVector { mThisEventSys.mCallbackMap[typeid(EventType)] };
            auto subID { mNextSubscriptionID++ };
            callbackVector.emplace_back(std::move(callback), subID);

            return subID;
        }

        //returns true if a subscription callback was successfully removed from the event system otherwise returns false
        template <typename EventType>
        bool unsub(SubscriptionID subID)
        {
            static_assert
            (
                IsTypeInPack<EventType, EventTs...>, 
                "The template type paramater passed to"
                " EventSystem::Subscriber::unsub was not a valid event type for this EventSystem."
            );

            return unsub(subID, typeid(EventType));
        }

    private:

        template <typename Enum, typename>
        requires std::is_enum_v<Enum>
        friend class SubscriptionManager;

        bool unsub(SubscriptionID ID, std::type_index eventTypeIdx) 
        {
            auto it { mThisEventSys.mCallbackMap.find(eventTypeIdx) };
            if(it != mThisEventSys.mCallbackMap.end())
            {
                auto& callbackVector { it->second };
                
                //remove the callback associated with this subID.
                bool wasCallbackErased = std::erase_if(callbackVector, [ID](auto const& callbackIDPair){
                    return callbackIDPair.second == ID;
                });

                //if that was the last subscription callback in this vector then remove it from the map
                if(callbackVector.empty())
                    mThisEventSys.mCallbackMap.erase(it);

                return wasCallbackErased;
            }

            return false;
        }

        friend class EventSystem<EventTs...>;
        Subscriber(EventSystem<EventTs...>& thisEventSys) : mThisEventSys{thisEventSys} {}

        EventSystem<EventTs...>& mThisEventSys;
        SubscriptionID mNextSubscriptionID {0};
    };

    struct Publisher
    {
        template <typename EventType>
        void pub(EventType& e) const
        {
            static_assert
            (
                IsTypeInPack<EventType, EventTs...>,
                "The template type paramater passed to"
                " EventSystem::pub was not a valid event type for this EventSystem."
            );
        
            //find the list of callbacks associated with this event type (if any)
            auto const it { mThisEventSys.mCallbackMap.find(typeid(e)) };
            
            if(it != mThisEventSys.mCallbackMap.end())
            {
                for(auto const& callbackAndIDPair : it->second)
                    callbackAndIDPair.first(e);
            }
        }

    private:

        friend class EventSystem<EventTs...>;
        Publisher(EventSystem<EventTs...> const& thisEventSys) : mThisEventSys{thisEventSys} {}

        EventSystem<EventTs...> const& mThisEventSys;
    };

    friend struct Subscriber;
    friend struct Publisher;

private:
    //A map from event types -> a list of subscription callbacks.
    std::unordered_map<std::type_index, 
        std::vector<std::pair<OnEventCallback, SubscriptionID>>> mCallbackMap;

    //use getSubscriber()/getPublisher() to get access to these, allowing the 
    //user of this event system to sub/unsub or publish events respectively.
    Subscriber mSubscriber {*this};
    Publisher  mPublisher  {*this};
};