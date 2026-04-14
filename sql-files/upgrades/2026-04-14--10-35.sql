ALTER TABLE `autotrade_merchants`
  ADD COLUMN `extended_vending_item` INT NOT NULL DEFAULT '0' AFTER `title`;