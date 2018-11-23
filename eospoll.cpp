/**
 * *  The apply() methods must have C calling convention so that the blockchain can lookup and
 * *  call these methods.
 * */
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/crypto.h>
#include <string>

extern "C" {
	struct transferargs {
	   account_name    from;
	   account_name    to;
	   eosio::asset    quantity;
	   std::string     memo;
	};

	struct revealargs {
	   uint64_t        tip;
	};

    struct offerbet {
    	uint64_t         id;
    	account_name     from;
    	account_name     to;
    	eosio::asset     quantity;
    	std::string      memo;
    	uint64_t primary_key() const {return id;}
    };
    typedef eosio::multi_index<N(offerbet), offerbet> offerbet_index;

	struct betnumber {
	   uint64_t          id;
	   uint64_t          offerbetid;
	   uint64_t          number;
	   uint64_t primary_key()const { return id; }
	};
	typedef eosio::multi_index< N(betnumber), betnumber> betnumber_index;

	struct globalindex {
	   uint64_t          id;
	   uint64_t          gindex;
	   uint64_t primary_key()const { return id; }
	};
	typedef eosio::multi_index< N(globalindex), globalindex> globalindex_index;

	struct betstate {
	   uint64_t          round;
	   uint64_t          total;
	   eosio::asset      quantity;
	   uint64_t          result;
	   uint64_t primary_key()const { return round; }
	};
	typedef eosio::multi_index< N(betstate), betstate> betstate_index;

	struct player {
		account_name      user;
		account_name      proxyer;
		account_name primary_key() const { return user; }
	};
	typedef eosio::multi_index< N(player), player> player_index;

	struct ledger {
		account_name      user;
		eosio::asset      quantity;
		account_name primary_key() const {return user;}
	};
	typedef eosio::multi_index< N(ledger), ledger> ledger_index;

   /// The apply method implements the dispatch of events to this contract
   void apply( uint64_t receiver, uint64_t code, uint64_t action ) {

	 //
	 auto _self = receiver;

	 //
	 globalindex_index globalindexs(_self, _self);
	 auto cur_globalindex_itr = globalindexs.find(N(bet));
	 if(cur_globalindex_itr == globalindexs.end()) {
		 cur_globalindex_itr = globalindexs.emplace(_self, [&](auto& info){
			 info.id       = N(bet);
			 info.gindex  = 0;
		 });
	 }

	 //
	 uint64_t bet_index = cur_globalindex_itr->gindex;

	 //
	 betstate_index betstates(_self, _self);
	 auto cur_betstate_itr = betstates.find(bet_index);
	 if(cur_betstate_itr == betstates.end()) {
		 cur_betstate_itr = betstates.emplace(_self, [&](auto& info){
			 info.round   = bet_index;
			 info.total   = 0;
			 info.result  = 0xFFFFFFFFFFFFFFFF;
		 });
	 }

	 //
	 uint64_t bet_total = cur_betstate_itr->total;
	 eosio::asset bet_quantity = cur_betstate_itr->quantity;

	 //
	 offerbet_index offerbets(_self, _self);
	 betnumber_index betnumbers(_self, _self);
	 player_index players(_self, _self);

	 //
	 if(code == N(eosio.token) && action == N(transfer)) {
		 transferargs args = eosio::unpack_action_data<transferargs>();
		 if(args.to != _self) {
			 return;
		 }

		auto sym = args.quantity.symbol;
		eosio_assert( sym.is_valid(), "invalid symbol name" );
		eosio_assert( sym == CORE_SYMBOL, "invalid symbol name" );

		eosio_assert( args.memo.size() <= 256, "memo has more than 256 bytes" );
	    eosio_assert( args.quantity.is_valid(), "invalid quantity" );
	    eosio_assert( args.quantity.amount > 0, "must issue positive quantity" );

		 auto new_offerbet_itr = offerbets.emplace(_self, [&](auto& info){
			info.id           = offerbets.available_primary_key();
			info.from         = args.from;
			info.to           = args.to;
			info.quantity     = args.quantity;
			info.memo         = args.memo;
		 });

		 // proxyer????
		 std::string memo = args.memo;
		 account_name proxyer = N(memo);
		 auto cur_proxyer_itr = players.find(proxyer);
		 if(cur_proxyer_itr == players.end()) {
			 proxyer = N(none);
		 } else {
			 //
		 }

		 //
		 auto cur_player_itr = players.find(args.from);
		 if(cur_player_itr == players.end()) {
			 cur_player_itr = players.emplace(_self, [&](auto& info){
				 info.user    = args.from;
				 info.proxyer = proxyer;
			 });
		 }

		 eosio::asset temp;
		 temp.set_amount(10000);

		 while(args.quantity >= temp) {
			 args.quantity = args.quantity - temp;

			 auto new_betnumber_itr = betnumbers.emplace(_self, [&](auto& info){
				info.id           = betnumbers.available_primary_key();
				info.offerbetid   = new_offerbet_itr->id;
				info.number       = info.id;
			 });

			 //
			 bet_total ++;
			 bet_quantity += temp;
		 }

		 //
		 betstates.modify( cur_betstate_itr, 0, [&](auto& info) {
			 info.total = bet_total;
			 info.quantity = bet_quantity;
		 });
	 }

	 //
	 if(code == N(eospool) && action == N(reveal)) {
		 revealargs args = eosio::unpack_action_data<revealargs>();

		 //
         uint32_t t_now = now();
         checksum256 hash;
         sha256((char*)(&t_now), sizeof(uint32_t), &hash);
         uint64_t bet_result = hash.hash[1];
         bet_result = bet_result << 32;
         bet_result |= hash.hash[0];
         bet_result = bet_result % cur_betstate_itr->total;
         //uint64_t bet_result = N(hash.hash);
         //bet_result = bet_result % cur_betstate_itr->total;
         //uint64_t bet_result = (hash.hash[15]) % cur_betstate_itr->total;

         //
		 betstates.modify( cur_betstate_itr, 0, [&](auto& info) {
			 info.result = bet_result;
		 });


         //
         ledger_index ledgers(_self, _self);
		 auto ledgers_itr = ledgers.begin();
		 while(ledgers_itr != ledgers.end()) {
			 ledgers_itr = ledgers.erase(ledgers_itr);
		 }

		 //
		 auto offerbet_itr = offerbets.begin();
		 while(offerbet_itr != offerbets.end()) {
			 account_name user = offerbet_itr->from;
			 eosio::asset amount = offerbet_itr->quantity;
			 offerbet_itr ++;

			 //
			 auto cur_player_itr = players.find(user);
			 account_name proxy1 = cur_player_itr->proxyer;
			 if(proxy1 == N(none)) {
				 continue;
			 }

			 //
			 auto cur_ledger_itr = ledgers.find(proxy1);
			 if(cur_ledger_itr == ledgers.end()) {
				 cur_ledger_itr = ledgers.emplace(_self, [&](auto& info) {
					 info.user       = proxy1;
					 info.quantity   = amount/10;
				 });
			 } else {
				 //
				 ledgers.modify( cur_ledger_itr, 0, [&](auto& info) {
					 info.quantity   += amount/10;
				 });
			 }

			 //
			 cur_player_itr = players.find(proxy1);
			 account_name proxy2 = cur_player_itr->proxyer;
			 if(proxy2 == N(none)) {
				 continue;
			 }

			 //
			 cur_ledger_itr = ledgers.find(proxy2);
			 if(cur_ledger_itr == ledgers.end()) {
				 cur_ledger_itr = ledgers.emplace(_self, [&](auto& info) {
					 info.user       = proxy2;
					 info.quantity   = (amount*5)/100;
				 });
			 } else {
				 //
				 ledgers.modify( cur_ledger_itr, 0, [&](auto& info) {
					 info.quantity   += (amount*5)/100;
				 });
			 }


			 //
			 cur_player_itr = players.find(proxy2);
			 account_name proxy3 = cur_player_itr->proxyer;
			 if(proxy3 == N(none)) {
				 continue;
			 }

			 //
			 cur_ledger_itr = ledgers.find(proxy3);
			 if(cur_ledger_itr == ledgers.end()) {
				 cur_ledger_itr = ledgers.emplace(_self, [&](auto& info) {
					 info.user       = proxy3;
					 info.quantity   = (amount*3)/100;
				 });
			 } else {
				 //
				 ledgers.modify( cur_ledger_itr, 0, [&](auto& info) {
					 info.quantity   += (amount*3)/100;
				 });
			 }
		 }

		 //
		 ledgers_itr = ledgers.begin();
		 eosio::asset proxy_total;
		 while(ledgers_itr != ledgers.end()) {
			 proxy_total += ledgers_itr->quantity;

	         eosio::action(
	            eosio::permission_level{ _self, N(active) },
	            N(eosio.token), N(transfer),
	            std::make_tuple(_self, ledgers_itr->user, ledgers_itr->quantity, std::string("bonus"))
	         ).send();

	         ledgers_itr ++;
		 }

		 //
         //
         auto prize_betnumber_itr = betnumbers.find(bet_result);
         auto prize_offerbet_itr = offerbets.find(prize_betnumber_itr->offerbetid);
		 eosio::asset prize_total = cur_betstate_itr->quantity - proxy_total;
		 prize_total = (prize_total*9)/10;

         eosio::action(
            eosio::permission_level{ _self, N(active) },
            N(eosio.token), N(transfer),
            std::make_tuple(_self, prize_offerbet_itr->from, prize_total, std::string("prize"))
         ).send();

         bet_index ++;
         globalindexs.modify(cur_globalindex_itr, 0, [&](auto& info) {
        	 info.gindex = bet_index;
         });

         {
        	 offerbet_index offetbets(_self, _self);
			  auto offetbets_itr = offetbets.begin();
			  while(offetbets_itr != offetbets.end()) {
					  offetbets_itr = offetbets.erase(offetbets_itr);
			  }
		  }

		  {
			  betnumber_index betnumbers(_self, _self);
			  auto betnumbers_itr = betnumbers.begin();
			  while(betnumbers_itr != betnumbers.end()) {
					  betnumbers_itr = betnumbers.erase(betnumbers_itr);
			  }
		  }

	 }

	 //
     if(code == N(eospool) && action == N(reset)) {
    	 {
    		 offerbet_index offetbets(_self, _self);
			 auto offetbets_itr = offetbets.begin();
			 while(offetbets_itr != offetbets.end()) {
				 offetbets_itr = offetbets.erase(offetbets_itr);
			 }
    	 }

    	 {
    		 betnumber_index betnumbers(_self, _self);
			 auto betnumbers_itr = betnumbers.begin();
			 while(betnumbers_itr != betnumbers.end()) {
				 betnumbers_itr = betnumbers.erase(betnumbers_itr);
			 }
    	 }

    	 {
    		 globalindex_index globalindexs(_self, _self);
			 auto globalindexs_itr = globalindexs.begin();
			 while(globalindexs_itr != globalindexs.end()) {
				 globalindexs_itr = globalindexs.erase(globalindexs_itr);
			 }
    	 }

    	 {
    		 betstate_index betstates(_self, _self);
			 auto betstates_itr = betstates.begin();
			 while(betstates_itr != betstates.end()) {
				 betstates_itr = betstates.erase(betstates_itr);
			 }
    	 }

    	 {
    		 player_index players(_self, _self);
			 auto players_itr = players.begin();
			 while(players_itr != players.end()) {
				 players_itr = players.erase(players_itr);
			 }
    	 }

    	 {
    		 ledger_index ledgers(_self, _self);
			 auto ledgers_itr = ledgers.begin();
			 while(ledgers_itr != ledgers.end()) {
				 ledgers_itr = ledgers.erase(ledgers_itr);
			 }
    	 }
     }
   }
} // extern "C"
